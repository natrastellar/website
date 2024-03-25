+++
title = "Rust-like Error Handling in C++"
date = 2024-03-21
summary = "Discussing C++23's std::expected."
tags = ["c++", "development"]
toc = true
+++

{{< note >}}
This article assumes background knowledge in C++, particularly C++17 standard library features and C++20 language features.
{{< /note >}}

I'm excited about C++23 for several reasons, but one of the biggest is the new header [\<expected\>](https://en.cppreference.com/w/cpp/header/expected), bringing with it one incredibly useful class template: `std::expected`.

## Wait, C++23? Isn't it 2024?

When it comes to new revisions of the C++ standard, Apple often lags behind Microsoft, particularly when it comes to some of the best new additions to the standard library, like format or ranges.
I came up with the idea for this article in November 2022.
At the time, GCC and MSVC had already implemented `<expected>`[^1], but we poor, unfortunate souls relying on Apple Clang had to wait.
In fact, we had to wait so long that I ended up implementing the header myself.
This wasn't too much of a burden since we *did* have access to C++20 concepts on all platforms at the time.
The benefits have been well worth the effort.

I originally intended the initial portion of this article to be about that implementation process.
However, as of a few weeks ago, Apple Clang finally supports [monadic operations for std::expected](https://wg21.link/P2505R5) in Xcode 15.3![^2]
This means implementing std::expected is unnecessary[^4] for teams that can update to the latest version of Xcode.
Additionally, [LWG 3836](https://cplusplus.github.io/LWG/issue3836) led to some important changes to the standard that weren't reflected in my original implementation.
I might talk about these changes in the future, but for now, I'm going to assume most people reading this aren't very interested in C++ standardese and instead focus on what `std::expected` is and how we can use it.

Let's start by talking about error handling before C++23.

[^1]: As of GCC 12 and Visual Studio 2022 17.3.
[^2]: See https://developer.apple.com/xcode/cpp/.
[^4]: It might still be useful for those who can't update, or are stuck using C++20 on any platform.

## A Simple Example - Renaming a File

Imagine we're working on a hypothetical desktop application that allows operating on files in various ways.
One of the operations we need to implement is renaming a chosen file on disk.
- Our implementation will, given an existing file, prompt the user for a new name and then attempt to rename the file using the `std::filesystem` library.
- The user can abort the operation when prompted for a new name.
- Should the operation succeed, we need to update the name in the UI.
- Alternatively, if the operation fails, we'd like to display a message to the user explaining the error.

Here's a small piece of what that could look like:

```cpp
#include <filesystem>
#include <string>

std::filesystem::path promptForFileName(const std::filesystem::path& oldPath) {
    const std::u8string oldStem = oldPath.stem().u8string();
    std::u8string newStem = ...; // Get user input
    if (newStem.empty()) {
        return {};
    }
    return oldPath.parent_path() / newStem / oldPath.extension();
}

bool renameFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath) {
    try {
        std::filesystem::rename(oldPath, newPath);
        return true;
    }
    catch (const std::filesystem::filesystem::error&) {
        return false;
    }
}

void onRenameSelectedFile(const std::filesystem::path& filePath) {
    const std::filesystem::path newPath = promptForFileName(filePath);
    if (newPath.empty()) {
        return;
    }
    if (renameFile(filePath, newPath)) {
        // Update the UI...
    }
    else {
        // Display error message...
    }
}
```

This is a good start, `renameFile` doesn't return any error information.
We'd like to provide some detail as to what went wrong.
There are a number of ways we could do that.

### Propagating Exceptions

The first is to propagate the exception:

```cpp
void onRenameSelectedFile(const std::filesystem::path& filePath) {
    //...
    try {
        std::filesystem::rename(oldPath, newPath);
        // Update the UI...
    }
    catch (const std::filesystem::error& e) {
        // Display error message...
    }
}
```

We've removed `renameFile` and made `onRenameSelectedFile` a little more complex.
Maybe we can tolerate that.

However, what if `renameFile` was used elsewhere and had additional logic that we didn't want to duplicate?

```cpp
bool renameFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath) {
    if (oldPath == newPath) {
        return false;
    }
    try {
        if (!std::filesystem::exists(oldPath)) {
            return false;
        }
        if (!std::filesystem::is_regular_file(oldPath)) {
            return false;
        }
        if (std::filesystem::exists(newPath)) {
            return false;
        }
        std::filesystem::rename(oldPath, newPath);
        return true;
    }
    catch (const std::filesystem::error&) {
        return false;
    }
}
```

Propagating the exception is unlikely to give enough indication as to what went wrong.
We could throw instead of returning false in the early return statements, but that doesn't seem like a good use of exceptions.
These are anticipated conditions, not truly exceptional scenarios.

### Out Parameters

We *could* use the overloads of these functions that take a reference to std::error_code.

```cpp
std::error_code renameFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath) {
    if (oldPath == newPath) {
        return {};
    }
    std::error_code ec;
    if (!std::filesystem::exists(oldPath, ec) || ec) {
        return ec;
    }
    if (!std::filesystem::is_regular_file(oldPath, ec) || ec) {
        return ec;
    }
    if (std::filesystem::exists(newPath, ec) || ec) {
        return ec;
    }
    std::filesystem::rename(oldPath, newPath, ec);
    return ec;
}

void onRenameSelectedFile(const std::filesystem::path& filePath) {
    //...
    const std::error_code ec = renameFile(filePath, newPath);
    if (!ec) { // Wait, was the file actually renamed...?
        // Update the UI...
    }
    else {
        // Display error message...
    }
}
```

Now we've lost information.
Callers can tell whether a `std::filesystem` call failed, but they can't tell whether the rename was performed.

Since `std::error_code` isn't an expensive type to construct, let's try passing it in by non-const reference as an "out" parameter, and go back to returning a bool to indicate whether the rename succeeded.

```cpp
bool renameFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath, std::error_code& ec) {
    ec.clear();
    if (oldPath == newPath) {
        return false;
    }
    if (!std::filesystem::exists(oldPath, ec) || ec) {
        return false;
    }
    if (!std::filesystem::is_regular_file(oldPath, ec) || ec) {
        return false;
    }
    if (std::filesystem::exists(newPath, ec) || ec) {
        return false;
    }
    std::filesystem::rename(oldPath, newPath, ec);
    return true;
}

void onRenameSelectedFile(const std::filesystem::path& filePath) {
    //...
    std::error_code ec;
    if (renameFile(filePath, newPath, ec)) {
        // Update the UI...
    }
    else if (ec) {
        // Display error message...
    }
}
```

One problem with this approach is that out parameters and in-out parameters are hard to disambiguate.
Another is that it adds a burden to the caller to initialize the parameter correctly.
If our function took more parameters than just two, adding yet another one might not be ideal.
If our error type was more difficult to construct, or wasn't intended to ever be constructed by client code, we might have to find another solution.

### Returning a std::pair

Let's instead try returning two pieces of information.
The first item will be whether or not the rename was attempted.
The second will be the error code, if an error occurred.

```cpp
std::pair<bool, std::error_code> renameFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath) {
    std::error_code ec;
    if (oldPath == newPath) {
        return {false, ec};
    }
    if (!std::filesystem::exists(oldPath, ec) || ec) {
        return {false, ec};
    }
    if (!std::filesystem::is_regular_file(oldPath, ec) || ec) {
        return {false, ec};
    }
    if (std::filesystem::exists(newPath, ec) || ec) {
        return {false, ec};
    }
    std::filesystem::rename(oldPath, newPath, ec);
    if (ec) {
        return {false, ec};
    }
    return {true, ec};
}

void onRenameSelectedFile(const std::filesystem::path& filePath) {
    //...
    const auto [renamed, ec] = renameFile(filePath, newPath);
    if (renamed) {
        // Update the UI...
    }
    else if (ec) {
        // Display error message...
    }
}
```

If `renameFile` returns false and no `std::filesystem` call fails, we still don't have information on why.
Additionally, we've muddied our API.
It's not entirely clear what the `bool` in the pair represents at a glance.
To be sure, we'd need to be able to see the function implementation or hope the function return value is documented.

### Returning a struct

We could address this latter problem by creating our own self-documenting struct:

```cpp
struct RenameResult {
    bool renamed = false;
    std::error_code ec;
}

RenameResult renameFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath) {
    // We can use the same syntax as before.
    //...
}

void onRenameSelectedFile(const std::filesystem::path& filePath) {
    //...
    const RenameResult result = renameFile(filePath, newPath);
    if (result.renamed) {
        // Update the UI...
    }
    else if (result.ec) {
        // Display error message...
    }
}
```

That's a little better.
Now, let's provide more information to callers of `renameFile` when `RenameResult::renamed` is false.

```cpp
enum class RenameErrorKind {
    None,
    FileDoesNotExist,
    InvalidFileType,
    FileAlreadyExists,
    Other,
}

struct RenameResult {
    bool renamed = false;
    RenameErrorKind error = RenameErrorKind::None;
    std::error_code ec;
}

RenameResult renameFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath) {
    std::error_code ec;
    if (oldPath == newPath) {
        return {false, RenameErrorKind::None, ec};
    }
    if (!std::filesystem::exists(oldPath, ec) || ec) {
        return {false, RenameErrorKind::FileDoesNotExist, ec};
    }
    if (!std::filesystem::is_regular_file(oldPath, ec) || ec) {
        return {false, RenameErrorKind::InvalidFileType, ec};
    }
    if (std::filesystem::exists(newPath, ec) || ec) {
        return {false, RenameErrorKind::FileAlreadyExists, ec};
    }
    std::filesystem::rename(oldPath, newPath, ec);
    if (ec) {
        return {false, RenameErrorKind::Other, ec};
    }
    return {true, RenameErrorKind::None, ec};
}
```

This is sufficient for our purposes.
`RenameResult` still isn't perfectly clear, though.
`RenameResult::renamed` could theoretically be true even if the result contains an error.
`renameFile` will never return such an object, but callers don't know that.

### Returning a std::variant

It would be better if a successful rename and an error were mutually exclusive.
We could change the struct into a class and establish this as an invariant.
This would also let us remove `RenameErrorKind::None`.
However, we might want to instead use something like `std::variant` to be more memory-efficient and to avoid having to write a bespoke type.
Let's do that for demonstration purposes.

```cpp
enum class RenameErrorKind {
    FileDoesNotExist,
    InvalidFileType,
    FileAlreadyExists,
    Other,
}

struct RenameError {
    RenameErrorKind error = RenameErrorKind::None;
    std::error_code ec;
}

std::variant<bool, RenameError> renameFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath) {
    if (oldPath == newPath) {
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::exists(oldPath, ec) || ec) {
        return RenameError{RenameErrorKind::FileDoesNotExist, ec};
    }
    if (!std::filesystem::is_regular_file(oldPath, ec) || ec) {
        return RenameError{RenameErrorKind::InvalidFileType, ec};
    }
    if (std::filesystem::exists(newPath, ec) || ec) {
        return RenameError{RenameErrorKind::FileAlreadyExists, ec};
    }
    std::filesystem::rename(oldPath, newPath, ec);
    if (ec) {
        return RenameError{RenameErrorKind::Other, ec};
    }
    return true;
}

void onRenameSelectedFile(const std::filesystem::path& filePath) {
    //...
    const std::variant<bool, RenameError> result = renameFile(filePath, newPath);
    if (const bool* renamed = std::get_if<bool>(&result)) {
        if (*renamed) {
            // Update the UI...
        }
    }
    else {
        const RenameError& error = std::get<RenameError>(result);
        // Display error message...
    }
}
```

We're more memory-efficient now, and the success/error invariant is established.
Unfortunately, `std::variant` doesn't lend itself well to simple code.
I find both `std::get_if` and `std::visit` feel a little clunky, given that they're non-member functions[^5].
If `renameFile` almost always succeeds, we're adding a lot of code to every caller.

Another thing to keep in mind when using `std::variant` is that there's often the rare possibility of an object entering a valueless state, should an exception be thrown during assignment or within a call to emplace.
We'd prefer not to ever have to think about that when writing code.

[^5]: This is changing in C++26 with the addition of [member visit](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2637r3.html)!

It would be nice if we had a type-safe union like `std::variant`, but with syntax and behavior tailored to our use case: a type that could hold either a "successful" value or an error.
Turns out that's exactly what we get with `std::expected`!

## Inspecting std::expected

`std::expected` isn't too conceptually complicated, but it provides a lot of syntactic sugar, *and* it tries to be as zero-cost as possible.
That means the class template definition is pretty long.
Let's look at it in smaller pieces.

### Expect the std::unexpected

Before we can begin, we have to talk about its little sibling, `std::unexpected`.

```cpp
// Requires clauses and noexcept operator expressions have been omitted for readability.
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
- It can be constructed using the forwarding constructor or in-place any number of arguments, preceded by an optional `std::initializer_list`.
  All constructors allow for conversions.
- It's copyable, it's movable, and it's swappable.
- The inner value can be accessed freely via the `error()` member function.
- It can be compared against another `std::unexpected` object - which may or may not have the same type - to see if the two are equal.
- All of its member functions are usable at compile time. Hurray!

{{< note >}}
There are some additional limitations on `E`:
- It can't be an array type.
- It must be an object type.
- It can't be another specialization of `std::unexpected`.
- It can't be cv-qualified.

In other words, none of the following instantiations will compile:
```cpp
auto unex1 = std::unexpected<int[]>{[2, 3]};
int num = 2;
auto unex2 = std::unexpected<int&>{num};
auto unex3 = std::unexpected<std::unexpected<int>>{std::unexpected<int>{2}};
auto unex4 = std::unexpected<const int>{2};
```
{{< /note >}}

Ultimately, `std::unexpected` doesn't do all that much for us on its own.
Let's return to `std::expected` for now.
We'll see why it's useful shortly.

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

To start with, the template takes two type parameters: `T` and `E`, corresponding to a successful value - or "expected" - type and an error type, respectively.
`std::expected<T, E>` can be thought of as a wrapper type similar to a `std::variant<T, E>`, also acting as a type-safe union, but serving a more narrow purpose and ending up much more user-friendly.

`T` can be `void` or any destructible type (not an array or reference).
`E` must be destructible and legal to use in `std::unexpected`.

{{< note >}}
`std::expected<void, E>` behaves rather similarly to `std::optional<E>`, but with seemingly inverted logic; for example, `has_value()`, which we'll see later, indicates that there *isn't* an error.
{{< /note >}}

Additionally, we have a few type aliases.
These come in handy when writing templates.

#### Construction

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
- `std::expected` is default-constructible, provided the value type `T` is, and will default to holding an instance of the value type.
- It is also copyable and movable, provided both contained types are.
- There are converting constructors to the value type, from other `std::expected` objects, and from `std::unexpected`.
- Both the value type *and* the error type can be constructed in-place.
  The latter uses a new in-place construction tag, `std::unexpect_t`, and the constant `std::unexpect`, which are analogous to `std::in_place_t` and `std::in_place`.
    ```cpp
    struct unexpect_t {
        explicit unexpect_t() = default;
    };
    inline constexpr unexpect_t unexpect{};
    ```

Here are some examples:
```cpp
std::expected<void, std::error_code> val1;
std::expected<double, std::error_code> val2; // Holds 0.0
auto val3 = std::expected<double, int>(2.5);
std::expected<std::string, int> val4("hello world");
std::expected<std::string, int> val5 = std::unexpected<int>(3);
auto val6 = std::expected<double, int>(std::unexpect, 3);
```

#### Assignment

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

Next up, the assignment operators.
They're pretty similar to the constructors.
We can assign a new value to a `std::expected` object, or a new error, again via `std::unexpected`.

You might have already picked up on why `std::unexpected` is useful: it indicates to the compiler and the reader that we're working with an error, rather than the successful value.
It also allows implicitly constructing a `std::expected` object holding an error, rather than having to explicitly construct one and pass `std::unexpect`.

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

Considering `std::unexpected` is swappable, it probably won't be very surprising that `std::expected` is swappable too.
We can also `emplace()` a value, much like `std::optional` or `std::variant`.

Here are some examples of assignment:
```cpp
std::expected<double, std::error_code> val1; // Holds 0.0
val1 = 3.5;

std::expected<std::string, int> val2;
val2 = std::unexpected<int>(3);

val1.emplace(1);
```

#### Accessors

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

Being dereferenceable, convertible to a bool, and exposing a `value()`, this portion of `std::expected`'s interface is similar to `std::optional`'s.
However, it has something `std::optional` doesn't: `error()`.
You'll notice that `error()` is similar to `std::unexpected::error()`.

{{< note >}}
Note that neither `operator*()` nor `error()` check whether the object contains the right type!
Calling them incorrectly will result in undefined behavior.
On the other hand, `value()` will throw an exception (`std::bad_expected_access`) if the object contains an error.
Again, this is pretty similar to `std::optional`.
{{< /note >}}

It's worth pointing out that there's an asymmetry in the parts of the interface we've seen so far.
It's easier to construct, emplace into, or access the inner type of a `std::expected` that holds an expected value, rather than an error.
In other words, `std::expected<T, E>` behaves more like a `T` than an `E` and requires less typing when it holds the former.

#### Comparison

```cpp
template<class T, class E>
class expected {
    /*...*/
    template<class T2, class E2> requires (/*...*/)
    friend constexpr bool operator==(const expected& x, const expected<T2, E2>& y);
    template<class T2> friend constexpr bool operator==(const expected& x, const T2& y);
    template<class E2> friend constexpr bool operator==(const expected& x, const unexpected<E2>& y);
    /*...*/
};
```

Finally, we have the equality operators.
Conveniently, we can compare a `std::expected` with a compatible `std::expected`, any type convertible to the value type, or even a `std::unexpected`!

This means we often won't have to dereference an object to compare it with a specific value:
```cpp
std::expected<double, std::error_code> val = 3.5;
assert(val == 3.5);
assert(val != std::unexpected{std::error_code{}});
```

### Returning to Our Example

Let's rework `renameFile` from earlier, but now using `std::expected`!

```cpp
std::expected<bool, RenameError> renameFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath) {
    if (oldPath == newPath) {
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::exists(oldPath, ec) || ec) {
        return std::unexpected(RenameError{RenameErrorKind::FileDoesNotExist, ec});
    }
    if (!std::filesystem::is_regular_file(oldPath, ec) || ec) {
        return std::unexpected(RenameError{RenameErrorKind::InvalidFileType, ec});
    }
    if (std::filesystem::exists(newPath, ec) || ec) {
        return std::unexpected(RenameError{RenameErrorKind::FileAlreadyExists, ec});
    }
    std::filesystem::rename(oldPath, newPath, ec);
    if (ec) {
        return std::unexpected(RenameError{RenameErrorKind::Other, ec});
    }
    return true;
}

void onRenameSelectedFile(const std::filesystem::path& filePath) {
    //...
    const std::expected<bool, RenameError> renamed = renameFile(filePath, newPath);
    if (renamed) { // or renamed.has_value()
        if (*renamed) { // or renamed.value()
            // Update the UI...
        }
    }
    else {
        const RenameError& error = renamed.error();
        // Display error message...
    }
}
```

The only difference in the body of `renameFile` is that we now explicitly construct an instance of `std::unexpected` in the error cases.
Working with the return value in `onRenameSelectedFile` is much more straightforward.
Hopefully you'll agree that `std::expected` makes the code easier to write and read, especially for callers!

### Monadic Functions

The few functions remaining are what I consider to be one of the most exciting parts of `std::expected`.

```cpp
template<class T, class E>
class expected {
    /*...*/
    template<class U> constexpr T value_or(U&& default_value) const &;
    template<class U> constexpr T value_or(U&& default_value) &&;
    template<class G = E> constexpr E error_or(G&& default_error) const &;
    template<class G = E> constexpr E error_or(G&& default_error) &&;

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

`value_or` behaves just like `std::optional::value_or`: If there is a contained value, it will be returned.
Otherwise, the provided `default_value` will be returned.
`error_or` is similar, but returns the contained or provided error instead.

Next we have `and_then`, `or_else`, `transform`, and `transform_error`.
Depending on the state of the object, the contained object will either be forwarded unchanged, or transformed using the provided function.
`std::optional` gets its own counterparts to these in C++23, too.

The functions passed to `and_then` and `or_else` must return a specialization of `std::expected` with an identical error type or value type, respectively.
The functions passed to `transform` and `transform_error` must return a valid value type or error type, respectively (the types do not have to match `T` or `E`).
In other words, `and_then` and `transform` can change `T`, while `or_else` and `transform_error` can change `E`.

These are useful when performing multiple operations sequentially, each depending on the success of the prior.
For example:

```cpp
std::expected<std::filesystem::path, Error> complexOperation() {
    return stepOne()
        .and_then(stepTwo) // stepTwo receives the value from stepOne
        .and_then(stepThree) // stepThree receives the value from stepTwo
        .or_else(fallback); // fallback receives an error returned from any of the steps and might also return an error
}

std::filesystem::path failsafeComplexOperation() {
    return complexOperation().value_or(/*some default path...*/);
}
```

In practice, using the monadic interface isn't quite so pretty as this example, since you're likely to run into scenarios where you have to capture a local variable and pass a lambda.
Still, it's a very welcome addition!
Without it, the above code would have to look something like this:

```cpp
std::expected<void, Error> complexOperation() {
    auto result = stepOne();
    if (!result) {
        return fallback(std::move(result.error()));
    }
    auto result2 = stepTwo(std::move(*result));
    if (!result2) {
        return fallback(std::move(result2.error()));
    }
    auto result3 = stepThree(std::move(*result2));
    if (!result3) {
        return fallback(std::move(result3.error()));
    }
    return result3;
}

std::filesystem::path failsafeComplexOperation() {
    auto result = complexOperation();
    if (!result) {
        return /*some default path...*/;
    }
    return std::move(*result);
}
```

## What Makes This Rust-like?

`std::expected` is comparable to Rust's [Result](https://doc.rust-lang.org/std/result/enum.Result.html).
- Both hold a `T` or an `E`.
- Both represent a success or an error.
- Both have monadic functions that allow operating on the contained value or error with a callable type.

`Result` has a lot of additional functionality that `std::expected` doesn't, though.
The C++ type lacks equivalents of `Result::and` or `Result::or`, which take a new `Result` instead of a callable type.
There's no built in method to convert a `std::expected` to a `std::optional` and vice versa, let alone transpose a `std::expected<std::optional<T>, E>` to/from a `std::optional<std::expected<T, E>>`.
Personally, I've often found myself wishing `std::expected` had the equivalent of `Result::inspect_err` when trying to log additional information about errors.

The end result of these differences - in addition to the verbosity of lambdas in C++ - is that working with `std::expected`'s monadic interface can sometimes make code more difficult to read or modify.
That's far from ideal, especially considering using `std::expected` in a pre-C++23 codebase might already require a significant refactoring project!

## Where To Go From Here

If you have compiler support for `std::expected`, maintaining a new or existing[^3] implementation simply to make up for functionality that C++ lacks may or may not be worth it.
It's a lot easier to focus on writing extensible error types and making it easier to debug or log information about an application.
In the future, I'd like to write a continuation of this article that discusses both of these topics!

As I mentioned in the beginning of this article, I'd also like to go over the implementation details of `std::expected` and fill in all of those `requires` clauses I omitted above.
You might be surprised by some of them!

In the meantime, if you work with C++, I hope you are able to make use of `std::expected` in your projects!

[^3]: Consider [Sy Brand's implementation](https://github.com/TartanLlama/expected), which works in C++11/14/17.