#pragma once

#include "expected.h"

namespace mori {
    class Error {};

    template<class T>
    using Result = expected<T, Error>;
}

#define Ok(...) {std::in_place __VA_OPT__(, ) __VA_ARGS__}
#define Err(...) {unexpect __VA_OPT__(, ) __VA_ARGS__}
