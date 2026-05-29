#include <cstdint>

int main() {
    std::int64_t result = 0;
    std::int64_t i = 0;
    std::int64_t even = 1;
    std::int64_t odd = 3;

    while (i < 40000000) {
        if ((i & 1) == 0) {
            result = result + even;
            even = even + 3;
        } else {
            result = result + odd;
            odd = odd + 5;
        }
        i = i + 1;
    }

    return static_cast<int>(result);
}
