+++
title = "Rust-like Error Handling in C++"
date = 2024-03-20
summary = "Writing ergonomic Result types."
tags = ["c++", "development"]
draft = true
toc = true
+++

{{% note %}}
This article assumes background knowledge in C++, particular C++17 standard library features and C++20 language features.
{{% /note %}}

I'm excited about C++23 for several reasons, but one of the biggest is the new header [\<expected\>](https://en.cppreference.com/w/cpp/header/expected), bringing with it one incredibly useful class template: `std::expected`.

## Wait, C++23? Isn't it 2024?

When it comes to new revisions of the C++ standard, Apple often lags behind Microsoft... particularly when it comes to some of the best new additions to the standard library, like format or ranges.
I came up with the idea for this post in November 2022. At the time, GCC and MSVC had already implemented \<expected\>[^1], but we poor, unfortunate souls relying on Apple Clang had to wait.
In fact, we had to wait so long that I ended up implementing the header myself.
This wasn't too much of a burden since we *did* have access to C++20 concepts on all platforms at the time.
The benefits have been well worth the effort.

I originally intended the initial portion of this post to be about that implementation process as well as an explanation of what `std::expected` is.
However, as of a few weeks ago, Apple Clang finally supports [monadic operations for std::expected](https://wg21.link/P2505R5) in Xcode 15.3![^2]
This means implementing std::expected is unnecessary for teams that can update to the latest version of Xcode.
Still, for those who can't update, or are stuck using C++20 on any platform, it might still be useful.
Additionally, [LWG 3836](https://cplusplus.github.io/LWG/issue3836) led to some important changes to the standard that weren't reflected in my original implementation.

Given all that, without further ado, let's start by taking a look at what C++23 gives us!

[^1]: As of GCC 12 and Visual Studio 2022 17.3.
[^2]: See https://developer.apple.com/xcode/cpp/.

## Expect the std::unexpected

Before we can talk about `std::expected`, we have to talk about its little sibling, `std::unexpected`.

```cpp
// Requires clauses and noexcept operator expressions have been omitted for brevity.
template<class E> requires (/*...*/)
class unexpected {
public:
    constexpr unexpected(const unexpected&);
    constexpr unexpected(unexpected&&);

    template<class Err = E> requires (/*...*/)
    constexpr explicit unexpected(Err&& e);

    template<class... Args> requires (/*...*/)
    constexpr explicit unexpected(std::in_place_t, Args&&... args);
    template<class U, class... Args> requires (/*...*/)
    constexpr explicit unexpected(std::in_place_t, std::initializer_list<U> il, Args&&... args);
    // Assignment operators and destructor are implicit

    constexpr const E& error() const & noexcept;
    constexpr E& error() & noexcept;
    constexpr const E&& error() const && noexcept;
    constexpr E&& error() && noexcept;

    template<class E2>
    friend constexpr bool operator==(const unexpected& x, const unexpected<E2>& y);

    constexpr void swap(unexpected& other) noexcept(/*...*/);
    friend constexpr void swap(unexpected& x, unexpected& y) noexcept(/*...*/);
};
```

`std::unexpected` is a fairly simple wrapper around an arbitrary error type, `E`.
- It can be constructed using the forwarding constructor or in-place any number of arguments, preceeded by an optional `std::initializer_list`. All constructors allow for conversions.
- It's copyable, it's movable, and it's swappable.
- The inner value can be accessed freely via the `error()` member function.
- It can be compared against another `std::unexpected` object, which may or may not have the same type.
- All of its member functions are usable at compile time. Hurray!

There are some additional limitations on `E`:
- It can't be an array type.
- It must be an object type.
- It can't be another specialization of `std::unexpected`.
- It can't be cv-qualified. In other words, the following code won't compile:
    ```cpp
    auto unex1 = std::unexpected<int[]>{[2, 3]};
    int num = 2;
    auto unex2 = std::unexpected<int&>{num};
    auto unex3 = std::unexpected<std::unexpected<int>>{std::unexpected<int>{2}};
    auto unex4 = std::unexpected<const int>{2};
    ```

Ultimately, `std::unexpected` doesn't do all that much for us on its own, other than potentially aid in code readability.
To really understand why `std::unexpected` exists, we have to look at `std::expected`.

## Inspecting the std::expected

`std::expected` isn't too conceptually complicated... but it provides a lot of sugar, *and* it tries to be as zero-cost as possible.
That means the class template definition is pretty long.
Let's look at it in smaller pieces.

### The Basics

```cpp
template<class T, class E>
class expected {
public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = unexpected<E>;

    template<class U>
    using rebind = expected<U, error_type>;
    /*...*/
};
```

To start with, the template takes two type parameters: `T` and `E`, corresponding to a value ("expected") type and error type, respectively.
`std::expected` can be thought of as a wrapper type similar to a `std::variant<T, E>`, also acting as a type-safe union, but much more user-friendly.[^3]
Additionally, we have a few type aliases.
These come in handy when writing templates.

[^3]: For instance... did you know that if an exception is thrown during initialization of the contained value during assignment or within a call to emplace, `std::variant` can end up holding no value?

```cpp
template<class T, class E>
class expected {
    /*...*/
    constexpr expected() requires (/*...*/);
    constexpr expected(const expected&) requires (/*...*/);
    constexpr expected(expected&&) noexcept(/*...*/) requires (/*...*/);

    template<class U, class G> requires (/*...*/)
    constexpr explicit(/*...*/) expected(const expected<U, G>& other);
    template<class U, class G> requires (/*...*/)
    constexpr explicit(/*...*/) expected(expected<U, G>&& other);

    template<class U = T> requires (/*...*/)
    constexpr explicit(/*...*/) expected(U&& v);

    template<class G> requires (/*...*/)
    constexpr explicit(/*...*/) expected(const unexpected<G>& e);
    template<class G> requires (/*...*/)
    constexpr explicit(/*...*/) expected(unexpected<G>&& e);

    template<class... Args> requires (/*...*/)
    constexpr explicit expected(std::in_place_t, Args&&... args);
    template<class U, class... Args> requires (/*...*/)
    constexpr explicit expected(std::in_place_t, std::initializer_list<U> il, Args&&... args);

    template<class... Args> requires (/*...*/)
    constexpr explicit expected(unexpect_t, Args&&... args);
    template<class U, class... Args> requires (/*...*/)
    constexpr explicit expected(unexpect_t, std::initializer_list<U> il, Args&&... args);

    constexpr ~expected();
    /***/
};
```

Next, we have the constructors and destructor.
From these, we can ascertain the following information:
- `std::expected` is default-constructible, provided the value type `T` is, and will default to holding an expected value.
- It is copyable and movable, provided both contained types are.
- There are converting constructors to the value type, from other `std::expected` objects, and from `std::unexpected`!
  More on this below.
- Both the value type *and* the error type can be constructed in-place.
  The latter uses a new in-place construction tag, `std::unexpect_t`, and the constant `std::unexpect`, which are analogous to `std::in_place_t` and `std::in_place`.
    ```cpp
    struct unexpect_t {
        explicit unexpect_t() = default;
    };
    inline constexpr unexpect_t unexpect{};
    ```

```cpp
template<class T, class E>
class expected {
    /*...*/
    constexpr expected& operator=(const expected& other) requires (/*...*/);
    constexpr expected& operator=(expected&& other) requires (/*...*/) noexcept(/*...*/);
    template<class U = T> requires (/*...*/)
    constexpr expected& operator=(U&& v);
    template<class G> requires (/*...*/)
    constexpr expected& operator=(const unexpected<G>& e);
    template<class G> requires (/*...*/)
    constexpr expected& operator=(unexpected<G>&& e);
    /*...*/
};
```

Here are the assignment operators.
They're pretty similar to the constructors.
We can assign a new value to a `std::expected` object, or a new error, again via `std::unexpected`.

```cpp
template<class T, class E>
class expected {
    /*...*/
    template<class... Args> requires (/*...*/)
    constexpr T& emplace(Args&&... args) noexcept;
    template<class U, class... Args> requires (/*...*/)
    constexpr T& emplace(std::initializer_list<U> il, Args&&... args) noexcept;

    constexpr void swap(expected& other) noexcept(/*...*/);
    friend constexpr void swap(expected& x, expected& y) noexcept(/*...*/);
    /*...*/
};
```

It probably won't be very surprising that `std::expected` is swappable.
We can also emplace a value, much like `std::optional` or `std::variant`.

### A Simple Example

You might be starting to see why `std::unexpected` is useful: it indicates to the compiler that we want to - often implicitly -  construct a `std::expected` object holding an error value.
This avoids having to explicitly construct a `std::expected` object and pass `std::unexpect`.
Here's an example of this in action:

```cpp
std::expected<bool, std::error_code> renameFile(
    const std::filesystem::path& oldFile,
    const std::filesystem::path& newFile) {
    if (oldFile == newFile) {
        return false;
    }
    std::error_code ec{};
    std::filesystem::rename(oldFile, newFile, ec);
    if (ec) {
        return std::unexpected(std::move(ec));
    }
    return true;
}
```

You might be wondering why `std::expected` benefits us here.
A caller of `renameFile` can easily determine:
1. Whether or not renaming the file succeeded.
2. Whether or not the file path was unchanged.
3. If renaming the file failed, what error code was returned.

Before `std::expected`, providing all of this information in the function return type alone might have been difficult or unwieldy!
Before, we'd have to either pass in an out parameter by reference, return a `std::pair`, return a `std::variant`, create a bespoke return type that encapsulates the same data, or throw an exception.
None of these options are ideal:
- Out parameters passed by reference aren't easily distinguishable from in-out parameters.
  Additionally, constructing both the success type and the failure type ahead of time might be expensive.
- `std::pair` isn't very readable at a glance.
  What is `first`? What is `second`?
  There's nothing indicating that only one of the two members should be used.
- `std::variant` requires lots of `std::get_if<...>()`.
- No one wants to litter the code with a bunch of different tiny classes/structs simply for returning multiple pieces of information... but when space isn't a concern, this might have been the most readable option in the past.
- Passing information by exceptions is not always possible, and even when it is, an uncaught exception could easily terminate the program.
  Throwing an exception solely to pass information places a much larger burden on callers. I believe that exceptions are best kept limited to truly exceptional situations, especially when a better alternative exists.

Now imagine how much more difficult it would have been if we needed to pass this error information way up the callstack. Especially so if we needed what the "success" type was.

### Accessors

```cpp
template<class T, class E>
class expected {
    /*...*/
    constexpr const T* operator->() const noexcept;
    constexpr T* operator->() noexcept;
    constexpr const T& operator*() const & noexcept;
    constexpr T& operator*() & noexcept;
    constexpr const T&& operator*() const && noexcept;
    constexpr T&& operator*() && noexcept;

    constexpr explicit operator bool() const noexcept;
    constexpr bool has_value() const noexcept;

    constexpr const T& value() const &;
    constexpr T& value() &;
    constexpr const T&& value() const &&;
    constexpr T&& value() &&;

    constexpr const E& error() const &;
    constexpr E& error() &;
    constexpr const E&& error() const &&;
    constexpr E&& error() &&;
    /*...*/
};
```

### Monadic Functions

```cpp
template<class T, class E>
class expected {
    /*...*/
    template<class U> constexpr T value_or(U&& v) const &;
    template<class U> constexpr T value_or(U&& v) &&;
    template<class G = E> constexpr E error_or(G&& e) const &;
    template<class G = E> constexpr E error_or(G&& e) &&;

    template<class F> constexpr auto and_then(F&& f) &;
    template<class F> constexpr auto and_then(F&& f) &&;
    template<class F> constexpr auto and_then(F&& f) const &;
    template<class F> constexpr auto and_then(F&& f) const &&;
    template<class F> constexpr auto or_else(F&& f) &;
    template<class F> constexpr auto or_else(F&& f) &&;
    template<class F> constexpr auto or_else(F&& f) const &;
    template<class F> constexpr auto or_else(F&& f) const &&;
    template<class F> constexpr auto transform(F&& f) &;
    template<class F> constexpr auto transform(F&& f) &&;
    template<class F> constexpr auto transform(F&& f) const &;
    template<class F> constexpr auto transform(F&& f) const &&;
    template<class F> constexpr auto transform_error(F&& f) &;
    template<class F> constexpr auto transform_error(F&& f) &&;
    template<class F> constexpr auto transform_error(F&& f) const &;
    template<class F> constexpr auto transform_error(F&& f) const &&;
    /***/
};
```

```cpp
template<class T, class E>
class expected {
    /*...*/
    template<class T2, class E2> requires (/*...*/)
    friend constexpr bool operator==(const expected& x, const expected<T2, E2>& y);
    template<class T2> friend constexpr bool operator==(const expected& x, const T2& y);
    template<class E2> friend constexpr bool operator==(const expected& x, const unexpected<E2>& y);
};
```

Whew, that was a lot of code.

## Mom, can we get std::result?

Describe what's so great about Rust result types

## Result at home

Implementing a result type

## Macro crimes

Ok(), Err()