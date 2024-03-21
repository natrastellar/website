#pragma once

#include <exception>
#include <type_traits>
#include <utility>
#include <variant>

namespace mori {
    template<class E>
    class unexpected {
    public:
        constexpr unexpected(const unexpected&) = default;
        constexpr unexpected(unexpected&&) noexcept = default;
        template<class Err = E>
        requires (!std::is_same_v<std::remove_cvref_t<Err>, unexpected>
            && !std::is_same_v<std::remove_cvref_t<Err>, std::in_place_t>
            && std::is_constructible_v<E, Err>)
        constexpr explicit unexpected(Err&& e) : unex(std::forward<Err>(e)) {}
        template<class... Args> requires (std::is_constructible_v<E, Args...>)
        constexpr explicit unexpected(std::in_place_t, Args&&... args) : unex(std::forward<Args>(args)...) {}
        template<class U, class... Args> requires (std::is_constructible_v<E, std::initializer_list<U>&, Args...>)
        constexpr explicit unexpected(std::in_place_t, std::initializer_list<U> il, Args&&... args) : unex(il, std::forward<Args>(args)...) {}

        [[nodiscard]] constexpr const E& error() const & noexcept { return unex; }
        [[nodiscard]] constexpr E& error() & noexcept { return unex; }
        [[nodiscard]] constexpr const E&& error() const && noexcept { return std::move(unex); }
        [[nodiscard]] constexpr E&& error() && noexcept { return std::move(unex); }

        template<class E2>
        [[nodiscard]] friend constexpr bool operator==(const unexpected& x, const unexpected<E2>& y) {
            return x.error() == y.error();
        }

        constexpr void swap(unexpected& other) noexcept(std::is_nothrow_swappable_v<E>) {
            using std::swap;
            swap(error(), other.error());
        }
        friend constexpr void swap(unexpected& x, unexpected& y) noexcept(noexcept(x.swap(y))) { x.swap(y); }

    private:
        E unex;
    };

    template<class E>
    unexpected(E) -> unexpected<E>;

    template<class E>
    class bad_expected_access : public bad_expected_access<void> {
    public:
        explicit bad_expected_access(E e) : unex(std::move(e)) {}

        [[nodiscard]] const E& error() const & noexcept { return unex; }
        [[nodiscard]] E& error() & noexcept { return unex; }
        [[nodiscard]] const E&& error() const && noexcept { return std::move(unex); }
        [[nodiscard]] E&& error() && noexcept { return std::move(unex); }

    private:
        E unex;
    };

    template<>
    class bad_expected_access<void> : public std::exception {
    public:
        const char* what() const noexcept override {
            return "incorrectly accessing an expected object that contains an unexpected value";
        }

    protected:
        bad_expected_access() noexcept = default;
        bad_expected_access(const bad_expected_access&) = default;
        bad_expected_access(bad_expected_access&&) noexcept = default;
        bad_expected_access& operator=(const bad_expected_access& other) = default;
        bad_expected_access& operator=(bad_expected_access&& other) noexcept = default;
        virtual ~bad_expected_access() noexcept(std::is_nothrow_destructible_v<std::exception>) = default;
    };

    struct unexpect_t {
        explicit unexpect_t() = default;
    };
    inline constexpr unexpect_t unexpect{};

    namespace detail {
        template<class T>
        constexpr bool is_unexpected_v = false;
        template<class E>
        constexpr bool is_unexpected_v<unexpected<E>> = true;

        template<std::size_t Index, class T, class E, class... Args>
        constexpr void reinit_expected(std::variant<T, E>& variant, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::variant_alternative_t<Index, std::variant<T, E>>, Args...>) {
            using NewType = std::variant_alternative_t<Index, std::variant<T, E>>;
            constexpr auto OldIndex = (Index == 0) ? std::size_t{1} : std::size_t{0};
            using OldType = std::variant_alternative_t<OldIndex, std::variant<T, E>>;
            if constexpr (std::is_nothrow_constructible_v<NewType, Args...>) {
                //std::destroy_at(std::addressof(old_val));
                //std::construct_at(std::addressof(new_val), std::forward<Args>(args)...);
                variant.template emplace<Index>(std::forward<Args>(args)...);
            }
            else if constexpr (std::is_nothrow_move_constructible_v<NewType>) {
                NewType temp(std::forward<Args>(args)...);
                //std::destroy_at(std::addressof(old_val));
                //std::construct_at(std::addressof(new_val), std::move(temp));
                variant.template emplace<Index>(std::move(temp));
            }
            else {
                OldType temp(std::move(variant.get<OldIndex));
                //std::destroy_at(std::addressof(old_val));
                try {
                    //std::construct_at(std::addressof(new_val), std::forward<Args>(args)...);
                    variant.template emplace<Index>(std::forward<Args>(args)...);
                }
                catch (...) {
                    //std::construct_at(std::addressof(old_val), std::move(temp));
                    variant.template emplace<OldIndex>(std::move(temp));
                    throw;
                }
            }
        }
    }

    template<class T, class E>
    class [[nodiscard]] expected {
    public:
        using value_type = T;
        using error_type = E;
        using unexpected_type = unexpected<E>;

        template<class U>
        using rebind = expected<U, error_type>;

        constexpr expected() requires (std::is_default_constructible_v<T>) = default;
        constexpr expected(const expected&) = delete;
        constexpr expected(const expected&)
            requires (std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>) = default;
        constexpr expected(expected&&)
            noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>)
            requires (std::is_move_constructible_v<T> && std::is_move_constructible_v<E>) = default;
        template<class U, class G>
        requires (std::is_constructible_v<T, std::add_lvalue_reference_t<const U>
            && std::is_constructible_v<E, const G&>
            && (std::is_same_v<std:remove_cv_t<T>, bool>
                || (!std::is_constructible_v<T, expected<U, G>&>
                    && !std::is_constructible_v<T, expected<U, G>>
                    && !std::is_constructible_v<T, const expected<U, G>&>
                    && !std::is_constructible_v<T, const expected<U, G>
                    && !std::is_convertible_v<expected<U, G>&, T>
                    && !std::is_convertible_v<expected<U, G>, T>
                    && !std::is_convertible_v<const expected<U, G>&, T>
                    && !std::is_convertible_v<const expected<U, G>, T>))
            && !std::is_constructible_v<unexpected<E>, expected<U, G>&>
            && !std::is_constructible_v<unexpected<E>, expected<U, G>
            && !std::is_constructible_v<unexpected<E>, const expected<U, G>&
            && !std::is_constructible_v<unexpected<E>, const expected<U, G>)
        constexpr explicit(!std::is_convertible_v<std::add_lvalue_reference_t<const U>, T> || !std::is_convertible_v<const G&, E>)
            expected(const expected<U, G>& other) : impl(other.has_value()
                ? std::variant<T, E>(std::in_place_index<0>, std::forward<std::add_lvalue_reference_t<const U>>(*other))
                : std::variant<T, E>(std::in_place_index<1>, std::forward<const G&>(other.error()))) {}
        template<class U, class G>
        requires (std::is_constructible_v<T, U>
            && std::is_constructible_v<E, G>
            && (std::is_same_v<std:remove_cv_t<T>, bool>
                || (!std::is_constructible_v<T, expected<U, G>&>
                    && !std::is_constructible_v<T, expected<U, G>>
                    && !std::is_constructible_v<T, const expected<U, G>&>
                    && !std::is_constructible_v<T, const expected<U, G>
                    && !std::is_convertible_v<expected<U, G>&, T>
                    && !std::is_convertible_v<expected<U, G>, T>
                    && !std::is_convertible_v<const expected<U, G>&, T>
                    && !std::is_convertible_v<const expected<U, G>, T>))
            && !std::is_constructible_v<unexpected<E>, expected<U, G>&>
            && !std::is_constructible_v<unexpected<E>, expected<U, G>
            && !std::is_constructible_v<unexpected<E>, const expected<U, G>&
            && !std::is_constructible_v<unexpected<E>, const expected<U, G>)
        constexpr explicit(!std::is_convertible_v<U, T> || !std::is_convertible_v<G, E>)
            expected(expected<U, G>&& other) : impl(other.has_value()
                ? std::variant<T, E>(std::in_place_index<0>, std::forward<U>(*other))
                : std::variant<T, E>(std::in_place_index<1>, std::forward<G>(other.error()))) {}
        template<class U = T>
        requires (!std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>
            && !std::is_same_v<expected, std::remove_cvref_t<U>>
            && std::is_constructible_v<T, U>
            && !detail::is_unexpected_v<std::remove_cvref_t<U>>
            && (!std::is_same_v<std::remove_cv_t<T>, bool> || !detail::is_unexpected_v<std::remove_cvref_t<U>>))
        constexpr explicit(!std::is_convertible_v<U, T>) expected(U&& v) : impl(std::in_place_index<0>, std::forward<U>(v)) {}
        template<class G>
        requires (std::is_constructible_v<E, const G&>)
        constexpr explicit(!std::is_convertible_v<const G&, E>) expected(const unexpected<G>& e) : impl(std::in_place_index<1>, std::forward<const G&>(e.error())) {}
        template<class G>
        requires (std::is_constructible_v<E, G>)
        constexpr explicit(!std::is_convertible_v<G, E>) expected(unexpected<G>&& e) : impl(std::in_place_index<1>, std::forward<G>(e.error())) {}
        template<class... Args>
        requires (std::is_constructible_v<T, Args...)
        constexpr explicit expected(std::in_place_t, Args&&... args) : impl(std::in_place_index<0>, std::forward<Args>(args)...) {}
        template<class U, class... Args>
        requires (std::is_constructible_v<T, std::initializer_list<U>&, Args...)
        constexpr explicit expected(std::in_place_t, std::initializer_list<U> il, Args&&... args) : impl(std::in_place_index<0>, il, std::forward<Args>(args)...) {}
        template<class... Args>
        requires (std::is_constructible_v<E, Args...)
        constexpr explicit expected(unexpect_t, Args&&... args) : impl(std::in_place_index<1>, std::forward<Args>(args)...) {}
        template<class U, class... Args>
        requires (std::is_constructible_v<E, std::initializer_list<U>&, Args...)
        constexpr explicit expected(unexpect_t, std::initializer_list<U> il, Args&&... args) : impl(std::in_place_index<1>, il, std::forward<Args>(args)...) {}
        constexpr expected& operator=(const expected& other) = delete;
        constexpr expected& operator=(const expected& other)
            requires (std::is_copy_assignable_v<T>
                && std::is_copy_constructible_v<T>
                && std::is_copy_assignable_v<E>
                && std::is_copy_constructible_v<E>
                && (std::is_nothrow_move_constructible_v<T>
                    || std::is_nothrow_move_constructible_v<E>)) {
            if (this != &other) {
                if (has_value() == other.has_value()) {
                    impl = other.impl;
                }
                else if (other.has_value()) {
                    detail::reinit_expected<0>(impl, *other);
                }
                else {
                    detail::reinit_expected<1>(impl, other.error());
                }
            }
            return *this;
        }
        constexpr expected& operator=(expected&& other)
            requires (std::is_move_assignable_v<T>
                && std::is_move_constructible_v<T>
                && std::is_move_assignable_v<E>
                && std::is_move_constructible_v<E>
                && (std::is_nothrow_move_constructible_v<T>
                    || std::is_nothrow_move_constructible_v<E>))
            noexcept(std::is_nothrow_move_constructible_v<T>
                && std::is_nothrow_move_assignable_v<T>
                && std::is_nothrow_move_constructible_v<E>
                && std::is_nothrow_move_assignable_v<E>) {
            if (this != &other) {
                if (has_value() == other.has_value()) {
                    impl = std::move(other.impl);
                }
                else if (other.has_value()) {
                    detail::reinit_expected<0>(impl, std::move(*other));
                }
                else {
                    detail::reinit_expected<1>(impl, std::move(other.error()));
                }
            }
            return *this;
        }
        template<class U = T>
        requires (!std::is_same_v<expected, std::remove_cvref_t<U>>
            && !detail::is_unexpected_v<std::remove_cvref_t<U>>
            && std::is_constructible_v<T, U>
            && std::is_assignable_v<T&, U>
            && (std::is_nothrow_constructible_v<T, U>
                || std::is_nothrow_move_constructible_v<T>
                || std::is_nothrow_move_constructible_v<E>))
        constexpr expected& operator=(U&& v) {
            if (has_value()) {
                **this = std::forward<U>(v);
            }
            else {
                detail::reinit_expected<0>(impl, std::forward<U>(v));
            }
        }
        template<class G>
        requires (std::is_constructible_v<E, const G&>
            && std::is_assignable_v<E&, const G&>
            && (std::is_nothrow_constructible_v<E, const G&>
                || std::is_nothrow_move_constructible_v<T> 
                || std::is_nothrow_move_constructible_v<E>))
        constexpr expected& operator=(const unexpected<G>& e) {
            if (has_value()) {
                detail::reinit_expected<1>(impl, std::forward<const G&>(e.error()));
            }
            else {
                error() = std::forward<const G&>(e.error());
            }
        }
        template<class G>
        requires (std::is_constructible_v<E, G>
            && std::is_assignable_v<E&, G>
            && (std::is_nothrow_constructible_v<E, G>
                || std::is_nothrow_move_constructible_v<T> 
                || std::is_nothrow_move_constructible_v<E>))
        constexpr expected& operator=(unexpected<G>&& e) {
            if (has_value()) {
                detail::reinit_expected<1>(impl, std::forward<G>(e.error()));
            }
            else {
                error() = std::forward<G>(e.error());
            }
        }
        constexpr ~expected() = default;

        template<class... Args>
        requires (std::is_nothrow_constructible_v<T, Args...>)
        constexpr T& emplace(Args&&... args) noexcept {
            impl.template emplace<0>(std::forward<Args>(args)...);
            return **this;
        }
        template<class U, class... Args>
        requires (std::is_nothrow_constructible_v<T, std::initializer_list<U>&, Args...>)
        constexpr T& emplace(std::initializer_list<U> il, Args&&... args) noexcept {
            impl.template emplace<0>(il, std::forward<Args>(args)...);
            return **this;
        }

        constexpr void swap(expected& other) noexcept();
        friend constexpr void swap(expected& x, expected& y) noexcept();

        [[nodiscard]] constexpr const T* operator->() const noexcept;
        [[nodiscard]] constexpr T* operator->() noexcept;
        [[nodiscard]] constexpr const T& operator*() const & noexcept;
        [[nodiscard]] constexpr T& operator*() & noexcept;
        [[nodiscard]] constexpr const T&& operator*() const && noexcept;
        [[nodiscard]] constexpr T&& operator*() && noexcept;
        [[nodiscard]] constexpr explicit operator bool() const noexcept;
        [[nodiscard]] constexpr bool has_value() const noexcept;
        [[nodiscard]] constexpr const T& value() const &;
        [[nodiscard]] constexpr T& value() &;
        [[nodiscard]] constexpr const T&& value() const &&;
        [[nodiscard]] constexpr T&& value() &&;
        [[nodiscard]] constexpr const E& error() const &;
        [[nodiscard]] constexpr E& error() &;
        [[nodiscard]] constexpr const E&& error() const &&;
        [[nodiscard]] constexpr E&& error() &&;
        template<class U>
        [[nodiscard]] constexpr T value_or(U&& v) const &;
        template<class U>
        [[nodiscard]] constexpr T value_or(U&& v) &&;
        template<class G = E>
        [[nodiscard]] constexpr E error_or(G&& e) const &;
        template<class G = E>
        [[nodiscard]] constexpr E error_or(G&& e) &&;

        template<class F>
        [[nodiscard]] constexpr auto and_then(F&& f) &;
        template<class F>
        [[nodiscard]] constexpr auto and_then(F&& f) &&;
        template<class F>
        [[nodiscard]] constexpr auto and_then(F&& f) const &;
        template<class F>
        [[nodiscard]] constexpr auto and_then(F&& f) const &&;
        template<class F>
        [[nodiscard]] constexpr auto or_else(F&& f) &;
        template<class F>
        [[nodiscard]] constexpr auto or_else(F&& f) &&;
        template<class F>
        [[nodiscard]] constexpr auto or_else(F&& f) const &;
        template<class F>
        [[nodiscard]] constexpr auto or_else(F&& f) const &&;
        template<class F>
        [[nodiscard]] constexpr auto transform(F&& f) &;
        template<class F>
        [[nodiscard]] constexpr auto transform(F&& f) &&;
        template<class F>
        [[nodiscard]] constexpr auto transform(F&& f) const &;
        template<class F>
        [[nodiscard]] constexpr auto transform(F&& f) const &&;
        template<class F>
        [[nodiscard]] constexpr auto transform_error(F&& f) &;
        template<class F>
        [[nodiscard]] constexpr auto transform_error(F&& f) &&;
        template<class F>
        [[nodiscard]] constexpr auto transform_error(F&& f) const &;
        template<class F>
        [[nodiscard]] constexpr auto transform_error(F&& f) const &&;

        template<class T2, class E2> requires (!std::is_void_v<T2>)
        [[nodiscard]] friend constexpr bool operator==(const expected& x, const expected<T2, E2>& y);
        template<class T2>
        [[nodiscard]] friend constexpr bool operator==(const expected& x, const T2& y);
        template<class E2>
        [[nodiscard]] friend constexpr bool operator==(const expected& x, const unexpected<E2>& y);

    private:
        std::variant<T, E> impl;
    };

    template<class T, class E> requires (std::is_void_v<T>)
    class [[nodiscard]] expected<T, E> {
    public:
        using value_type = T;
        using error_type = E;
        using unexpected_type = unexpected<E>;

        template<class U>
        using rebind = expected<U, error_type>;

        constexpr expected() noexcept;
        constexpr explicit() expected(const expected&);
        constexpr explicit() expected(expected&&) noexcept();
        template<class U, class G>
        constexpr explicit() expected(const expected<U, G>&);
        template<class U, class G>
        constexpr explicit() expected(expected<U, G>&&) noexcept();
        template<class G>
        constexpr expected(const unexpected<G>&);
        template<class G>
        constexpr expected(unexpected<G>&&) noexcept();
        constexpr explicit expected(std::in_place_t) noexcept;
        template<class... Args>
        constexpr explicit expected(unexpect_t, Args&&... args);
        template<class U, class... Args>
        constexpr explicit expected(unexpect_t, std::initializer_list<U> il, Args&&... args);
        constexpr expected& operator=(const expected& other);
        constexpr expected& operator=(expected&& other) noexcept();
        template<class G>
        constexpr expected& operator=(const unexpected<G>& other);
        template<class G>
        constexpr expected& operator=(unexpected<G>&& other) noexcept();
        constexpr ~expected() noexcept() = default;

        constexpr void emplace() noexcept;

        constexpr void swap(expected& other) noexcept();
        friend constexpr void swap(expected& x, expected& y) noexcept();

        [[nodiscard]] constexpr explicit operator bool() const noexcept;
        [[nodiscard]] constexpr bool has_value() const noexcept;
        [[nodiscard]] constexpr void operator*() const noexcept;
        [[nodiscard]] constexpr void value() const &;
        [[nodiscard]] constexpr void value() &&;
        [[nodiscard]] constexpr const E& error() const &;
        [[nodiscard]] constexpr E& error() &;
        [[nodiscard]] constexpr const E&& error() const &&;
        [[nodiscard]] constexpr E&& error() &&;
        template<class G = E>
        [[nodiscard]] constexpr E error_or(G&& e) const &;
        template<class G = E>
        [[nodiscard]] constexpr E error_or(G&& e) &&;

        template<class F>
        [[nodiscard]] constexpr auto and_then(F&& f) &;
        template<class F>
        [[nodiscard]] constexpr auto and_then(F&& f) &&;
        template<class F>
        [[nodiscard]] constexpr auto and_then(F&& f) const &;
        template<class F>
        [[nodiscard]] constexpr auto and_then(F&& f) const &&;
        template<class F>
        [[nodiscard]] constexpr auto or_else(F&& f) &;
        template<class F>
        [[nodiscard]] constexpr auto or_else(F&& f) &&;
        template<class F>
        [[nodiscard]] constexpr auto or_else(F&& f) const &;
        template<class F>
        [[nodiscard]] constexpr auto or_else(F&& f) const &&;
        template<class F>
        [[nodiscard]] constexpr auto transform(F&& f) &;
        template<class F>
        [[nodiscard]] constexpr auto transform(F&& f) &&;
        template<class F>
        [[nodiscard]] constexpr auto transform(F&& f) const &;
        template<class F>
        [[nodiscard]] constexpr auto transform(F&& f) const &&;
        template<class F>
        [[nodiscard]] constexpr auto transform_error(F&& f) &;
        template<class F>
        [[nodiscard]] constexpr auto transform_error(F&& f) &&;
        template<class F>
        [[nodiscard]] constexpr auto transform_error(F&& f) const &;
        template<class F>
        [[nodiscard]] constexpr auto transform_error(F&& f) const &&;

        template<class T2, class E2> requires (std::is_void_v<T2>)
        [[nodiscard]] friend constexpr bool operator==(const expected& x, const expected<T2, E2>& y);
        template<class E2>
        [[nodiscard]] friend constexpr bool operator==(const expected& x, const unexpected<E2>& y);

    private:
        std::variant<std::monostate, E> impl;
    };
}
