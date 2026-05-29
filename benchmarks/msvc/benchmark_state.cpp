#include <cstdint>

int main() {
    std::int64_t result = 0;
    std::int64_t i = 0;
    std::int64_t a = 1;
    std::int64_t b = 3;

    while (i < 50000000) {
        result = result + a;
        result = result + b;
        a = a + 3;
        b = b + 5;
        i = i + 1;
    }

    return static_cast<int>(result);
}
