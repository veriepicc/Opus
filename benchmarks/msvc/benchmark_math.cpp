#include <cstdint>

int main() {
    std::int64_t result = 0;
    std::int64_t i = 0;

    while (i < 80000000) {
        std::int64_t x = i * 8;
        std::int64_t y = x + 1;
        std::int64_t z = 0;
        result = result + y + z;
        i = i + 1;
    }

    return static_cast<int>(result);
}
