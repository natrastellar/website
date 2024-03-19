+++
title = "Rust-like Error Handling in C++"
date = 2022-11-05
summary = "Writing ergonomic Result types."
draft = true
+++

I'm excited about C++23 for several reasons, but one of the biggest is the juicy, new header [\<expected\>](https://en.cppreference.com/w/cpp/header/expected).

GCC and MSVC already implement this[^1], but we poor, unfortunate souls relying on Clang (or Apple Clang) can only look longingly toward the future.
Why wait? We have access to C++20 concepts; we can implement the entire header ourselves.

[^1]: As of GCC 12 and Visual Studio 2022 17.3.

## Expect the std::unexpected
## Expect the... std::expected
## Mom, can we get std::result?
## Result at home
## Where we're going we don't need the heap
## Macro crimes