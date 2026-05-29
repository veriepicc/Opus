#include <cstdint>

static int fold(int x, int bias) {
    int y = x * 17 + bias;
    if (y < bias) {
        y = y + 31;
    } else {
        y = y - 17;
    }
    if (y < 1000) {
        y = y + 7;
    } else {
        y = y - 5;
    }
    return y;
}

static int blend(int left, int right) {
    int v = left + right * 3;
    if (left < right) {
        v = v + left;
    } else {
        v = v - right;
    }
    return v;
}

int main() {
    int total = 0;
    int state = 7;
    int noise = 11;
    int i = 0;

    while (i < 6000000) {
        int a = fold(state, noise);
        int b = blend(a, noise);

        if (b < state) {
            total = total + a;
            noise = noise + 5;
        } else {
            total = total + b;
            noise = noise - 3;
        }

        if (i < noise) {
            total = total + state;
            state = state + 2;
        } else {
            total = total - noise;
            state = state + 1;
        }

        i = i + 1;
    }

    return total;
}
