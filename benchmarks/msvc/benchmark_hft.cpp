#include <cstdint>

struct Book {
    std::int64_t bid_px;
    std::int64_t bid_sz;
    std::int64_t ask_px;
    std::int64_t ask_sz;
    std::int64_t last_px;
    std::int64_t last_qty;
    std::int64_t seq;
};

struct Trader {
    std::int64_t inventory;
    std::int64_t cash;
    std::int64_t pnl;
    std::int64_t buy_px;
    std::int64_t sell_px;
    std::int64_t buy_qty;
    std::int64_t sell_qty;
    std::int64_t fills;
    std::int64_t rejects;
    std::int64_t risk;
};

static std::int64_t mix(std::int64_t i, std::int64_t salt) {
    std::int64_t x = i * 1103515245LL + salt * 2654435761LL;
    x = x ^ (x >> 13);
    x = x * 2246822519LL;
    return x & 1023;
}

static Book apply_market(Book book, std::int64_t event_id) {
    const std::int64_t bid_move = (mix(event_id, 17) & 7) - 3;
    const std::int64_t ask_move = (mix(event_id, 29) & 7) - 3;
    const std::int64_t bid_size_bump = (mix(event_id, 43) & 31) + 1;
    const std::int64_t ask_size_bump = (mix(event_id, 59) & 31) + 1;

    book.bid_px = book.bid_px + bid_move;
    book.ask_px = book.ask_px + ask_move;

    if (book.ask_px <= book.bid_px) {
        book.ask_px = book.bid_px + 1;
    }

    if ((event_id & 1) == 0) {
        book.bid_sz = book.bid_sz + bid_size_bump;
        book.ask_sz = book.ask_sz - (ask_size_bump >> 1);
    } else {
        book.ask_sz = book.ask_sz + ask_size_bump;
        book.bid_sz = book.bid_sz - (bid_size_bump >> 1);
    }

    if (book.bid_sz < 1) {
        book.bid_sz = 1 + (bid_size_bump & 15);
    }
    if (book.ask_sz < 1) {
        book.ask_sz = 1 + (ask_size_bump & 15);
    }

    book.last_px = (book.bid_px + book.ask_px) >> 1;
    book.last_qty = ((book.bid_sz ^ book.ask_sz) & 63) + 1;
    book.seq = book.seq + 1;
    return book;
}

static Trader quote_step(Trader trader, Book book, std::int64_t event_id) {
    book = apply_market(book, event_id);

    const std::int64_t spread = book.ask_px - book.bid_px;
    const std::int64_t mid = (book.ask_px + book.bid_px) >> 1;
    const std::int64_t imbalance = book.bid_sz - book.ask_sz;
    const std::int64_t skew = trader.inventory >> 2;
    const std::int64_t risk_push = trader.inventory * spread + imbalance;

    trader.buy_px = mid - 1 - skew;
    trader.sell_px = mid + 1 - skew;

    if (spread > 3) {
        trader.buy_px = trader.buy_px - 1;
        trader.sell_px = trader.sell_px + 1;
    }

    trader.buy_qty = 8 + ((book.bid_sz + imbalance) & 15);
    trader.sell_qty = 8 + ((book.ask_sz - imbalance) & 15);

    if (trader.inventory > 96) {
        trader.buy_qty = 1;
        trader.sell_qty = trader.sell_qty + 8;
        trader.risk = trader.risk + 3;
    } else if (trader.inventory < -96) {
        trader.sell_qty = 1;
        trader.buy_qty = trader.buy_qty + 8;
        trader.risk = trader.risk + 3;
    } else {
        trader.risk = trader.risk + (risk_push & 1);
    }

    const std::int64_t trigger = mix(event_id, trader.inventory + 71);
    if (trigger < 96) {
        trader.inventory = trader.inventory + trader.buy_qty;
        trader.cash = trader.cash - trader.buy_px * trader.buy_qty;
        trader.fills = trader.fills + 1;
    } else if (trigger > 928) {
        trader.inventory = trader.inventory - trader.sell_qty;
        trader.cash = trader.cash + trader.sell_px * trader.sell_qty;
        trader.fills = trader.fills + 1;
    } else {
        trader.rejects = trader.rejects + 1;
    }

    trader.pnl = trader.cash + trader.inventory * book.last_px - trader.risk * 17;
    return trader;
}

int main() {
    Book book{
        100000,
        24,
        100002,
        26,
        100001,
        4,
        1
    };

    Trader trader{
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    };

    std::int64_t checksum = 0;
    std::int64_t i = 0;

    while (i < 12000000) {
        trader = quote_step(trader, book, i);
        book = apply_market(book, i + trader.fills + 13);

        if (trader.inventory > 128) {
            trader.inventory = trader.inventory - 7;
            trader.cash = trader.cash + book.bid_px * 7;
            checksum = checksum + trader.inventory;
        } else if (trader.inventory < -128) {
            trader.inventory = trader.inventory + 7;
            trader.cash = trader.cash - book.ask_px * 7;
            checksum = checksum - trader.inventory;
        } else {
            checksum = checksum + trader.pnl + book.seq;
        }

        i = i + 1;
    }

    return static_cast<int>((checksum ^ trader.pnl ^ book.last_px) & 255);
}
