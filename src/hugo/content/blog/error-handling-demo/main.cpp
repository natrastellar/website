#include "result.h"

#include <cassert>

int main(int /*argc*/, char** /*argv*/) {
    mori::expected<double, int> ex = mori::unexpected(3);
    std::assert(!ex);
    std::assert(ex == mori::unexpected(3));
    ex.emplace(4.2);
    std::assert(ex);
    std::assert(ex == 4.2);

    return 0;
}
