+++
title = "Rust-like Error Handling in C++"
date = 2024-03-20
summary = "Writing ergonomic Result types."
tags = ["c++", "development"]
draft = true
+++

I'm excited about C++23 for several reasons, but one of the biggest is the new header [\<expected\>](https://en.cppreference.com/w/cpp/header/expected), bringing with it one incredibly useful class template: std::expected.

## Wait, C++23? Isn't it 2024?

When it comes to new revisions of the C++ standard, Apple often lags behind Microsoft... particularly when it comes to some of the best new additions to the standard library, like std::format or ranges.
I came up with the idea for this post in November 2022. At the time, GCC and MSVC had already implemented \<expected\>[^1], but we poor, unfortunate souls relying on Apple Clang had to wait.
In fact, we had to wait so long that I ended up implementing the header myself, since we *did* have access to C++20 concepts on all platforms.

I originally intended the initial portion of this post to be about that implementation process as well as an explanation of what std::expected is.
However, as of a few weeks ago, Apple Clang finally supports [monadic operations for std::expected](https://wg21.link/P2505R5) in Xcode 15.3[^2]!
This means implementing std::expected is unnecessary for teams that can update to the latest version of Xcode.
Still, for those who can't update, or are stuck using C++20 on any platform, it might still be useful.
Additionally, [LWG 3836](https://cplusplus.github.io/LWG/issue3836) led to some important changes to the standard that weren't reflected in my original implementation.
Given that, without further ado, let's take a look at what C++23 gives us!

[^1]: As of GCC 12 and Visual Studio 2022 17.3.
[^2]: See https://developer.apple.com/xcode/cpp/.

## Expect the std::unexpected

Implementing unexpected

## Expect the... std::expected

Implementing expected

## Mom, can we get std::result?

Describe what's so great about Rust result types

## Result at home

Implementing a result type

## Macro crimes

Ok(), Err()