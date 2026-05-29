#include <cstdint>

struct Agent {
    std::int64_t health;
    std::int64_t mana;
    std::int64_t x;
    std::int64_t y;
    std::int64_t score;
    std::int64_t cooldown;
};

int main() {
    Agent* a = new Agent{90, 45, 4, 7, 0, 0};
    Agent* b = new Agent{120, 30, 9, 3, 4, 1};
    Agent* c = new Agent{70, 80, 2, 11, 2, 2};
    Agent* d = new Agent{140, 25, 6, 5, 8, 0};
    std::int64_t total = 0;
    std::int64_t i = 0;

    while (i < 3000000) {
        const std::int64_t slot = i & 3;

        if (slot == 0) {
            const std::int64_t metric = a->health + a->mana + i;
            std::int64_t local = metric + b->cooldown;
            if (a->health < b->health) {
                a->score = a->score + metric;
                a->mana = a->mana + 2;
                b->health = b->health - 1;
                local = local + a->x;
            } else {
                a->score = a->score + a->cooldown;
                a->cooldown = a->cooldown + 1;
                a->x = a->x + 1;
                local = local - b->y;
            }
            if (a->cooldown < 3) {
                a->y = a->y + 1;
            } else {
                a->mana = a->mana - 1;
                a->y = a->y - 1;
            }
            total = total + local;
            if (a->health < c->health) {
                total = total + a->x + c->y;
                c->cooldown = c->cooldown + 1;
            } else {
                total = total - b->mana;
                d->score = d->score + 2;
            }
        } else {
            if (slot == 1) {
                const std::int64_t metric = b->health + b->mana + i;
                std::int64_t local = metric + c->cooldown;
                if (b->health < c->health) {
                    b->score = b->score + metric;
                    b->mana = b->mana + 2;
                    c->health = c->health - 1;
                    local = local + b->x;
                } else {
                    b->score = b->score + b->cooldown;
                    b->cooldown = b->cooldown + 1;
                    b->x = b->x + 1;
                    local = local - c->y;
                }
                if (b->cooldown < 3) {
                    b->y = b->y + 1;
                } else {
                    b->mana = b->mana - 1;
                    b->y = b->y - 1;
                }
                total = total + local;
                if (b->mana < d->mana) {
                    b->health = b->health + 1;
                    total = total + b->cooldown;
                } else {
                    c->score = c->score + b->x;
                    total = total - c->y;
                }
            } else {
                if (slot == 2) {
                    const std::int64_t metric = c->health + c->mana + i;
                    std::int64_t local = metric + d->cooldown;
                    if (c->health < d->health) {
                        c->score = c->score + metric;
                        c->mana = c->mana + 2;
                        d->health = d->health - 1;
                        local = local + c->x;
                    } else {
                        c->score = c->score + c->cooldown;
                        c->cooldown = c->cooldown + 1;
                        c->x = c->x + 1;
                        local = local - d->y;
                    }
                    if (c->cooldown < 3) {
                        c->y = c->y + 1;
                    } else {
                        c->mana = c->mana - 1;
                        c->y = c->y - 1;
                    }
                    total = total + local;
                    if (c->score < a->score) {
                        c->mana = c->mana + 3;
                        total = total + d->x;
                    } else {
                        a->health = a->health - 1;
                        total = total - a->cooldown;
                    }
                } else {
                    const std::int64_t metric = d->health + d->mana + i;
                    std::int64_t local = metric + a->cooldown;
                    if (d->health < a->health) {
                        d->score = d->score + metric;
                        d->mana = d->mana + 2;
                        a->health = a->health - 1;
                        local = local + d->x;
                    } else {
                        d->score = d->score + d->cooldown;
                        d->cooldown = d->cooldown + 1;
                        d->x = d->x + 1;
                        local = local - a->y;
                    }
                    if (d->cooldown < 3) {
                        d->y = d->y + 1;
                    } else {
                        d->mana = d->mana - 1;
                        d->y = d->y - 1;
                    }
                    total = total + local;
                    if (d->health < b->health) {
                        d->score = d->score + 5;
                        total = total + a->y;
                    } else {
                        b->mana = b->mana - 1;
                        total = total - d->x;
                    }
                }
            }
        }

        i = i + 1;
    }

    delete a;
    delete b;
    delete c;
    delete d;

    return static_cast<int>(total & 255);
}
