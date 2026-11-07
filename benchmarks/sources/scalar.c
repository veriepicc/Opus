#include <stdint.h>

#ifndef BENCH
#define BENCH 1
#elif BENCH == 5
static int64_t mix(int64_t i, int64_t salt) {
    int64_t x = i * 1103515245LL + salt * 2654435761LL;
    x = x ^ (x >> 13);
    x = x * 2246822519LL;
    return x & 1023;
}

int main(void) {
    int64_t bid_px = 100000, bid_sz = 24, ask_px = 100002, ask_sz = 26;
    int64_t last_px = 100001, last_qty = 4, seq = 1;
    int64_t inventory = 0, cash = 0, pnl = 0, buy_px = 0, sell_px = 0;
    int64_t buy_qty = 0, sell_qty = 0, fills = 0, rejects = 0, risk = 0, checksum = 0;
    for (int64_t i = 0; i < 12000000; i++) {
        int64_t bid_size_bump = (mix(i, 43) & 31) + 1;
        int64_t ask_size_bump = (mix(i, 59) & 31) + 1;
        bid_px += (mix(i, 17) & 7) - 3;
        ask_px += (mix(i, 29) & 7) - 3;
        if (ask_px <= bid_px) ask_px = bid_px + 1;
        if ((i & 1) == 0) { bid_sz += bid_size_bump; ask_sz -= ask_size_bump >> 1; }
        else { ask_sz += ask_size_bump; bid_sz -= bid_size_bump >> 1; }
        if (bid_sz < 1) bid_sz = 1 + (bid_size_bump & 15);
        if (ask_sz < 1) ask_sz = 1 + (ask_size_bump & 15);
        int64_t spread = ask_px - bid_px, mid = (ask_px + bid_px) >> 1;
        int64_t imbalance = bid_sz - ask_sz, skew = inventory >> 2;
        buy_px = mid - 1 - skew;
        sell_px = mid + 1 - skew;
        if (spread > 3) { buy_px--; sell_px++; }
        buy_qty = 8 + ((bid_sz + imbalance) & 15);
        sell_qty = 8 + ((ask_sz - imbalance) & 15);
        if (inventory > 96) { buy_qty = 1; sell_qty += 8; risk += 3; }
        else if (inventory < -96) { sell_qty = 1; buy_qty += 8; risk += 3; }
        else risk += (inventory * spread + imbalance) & 1;
        int64_t trigger = mix(i, inventory + 71);
        if (trigger < 96) { inventory += buy_qty; cash -= buy_px * buy_qty; fills++; }
        else if (trigger > 928) { inventory -= sell_qty; cash += sell_px * sell_qty; fills++; }
        else rejects++;
        last_px = mid;
        last_qty = ((bid_sz ^ ask_sz) & 63) + 1;
        seq++;
        pnl = cash + inventory * last_px - risk * 17;
        if (inventory > 128) { inventory -= 7; cash += bid_px * 7; checksum += inventory; }
        else if (inventory < -128) { inventory += 7; cash -= ask_px * 7; checksum -= inventory; }
        else checksum += pnl + seq + last_qty + rejects;
    }
    return (int)((checksum ^ pnl ^ last_px) & 255);
}
#endif

#if BENCH == 1
int main(void) {
    int64_t result = 0;
    int64_t i = 0;
    while (i < 80000000) {
        int64_t x = i * 8;
        int64_t y = x + 1;
        int64_t z = 0;
        result = result + y + z;
        i = i + 1;
    }
    return (int)result;
}
#elif BENCH == 2
int main(void) {
    int64_t result = 0;
    int64_t i = 0;
    int64_t a = 1;
    int64_t b = 3;
    while (i < 50000000) {
        result = result + a;
        result = result + b;
        a = a + 3;
        b = b + 5;
        i = i + 1;
    }
    return (int)result;
}
#elif BENCH == 3
int main(void) {
    int64_t result = 0;
    int64_t i = 0;
    int64_t even = 1;
    int64_t odd = 3;
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
    return (int)result;
}
#elif BENCH == 4
static int64_t fold(int64_t x, int64_t bias) {
    int64_t y = x * 17 + bias;
    if (y < bias) y = y + 31; else y = y - 17;
    if (y < 1000) y = y + 7; else y = y - 5;
    return y;
}

static int64_t blend(int64_t left, int64_t right) {
    int64_t v = left + right * 3;
    if (left < right) v = v + left; else v = v - right;
    return v;
}

int main(void) {
    int64_t total = 0;
    int64_t state = 7;
    int64_t noise = 11;
    int64_t i = 0;
    while (i < 6000000) {
        int64_t a = fold(state, noise);
        int64_t b = blend(a, noise);
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
    return (int)total;
}
#endif
