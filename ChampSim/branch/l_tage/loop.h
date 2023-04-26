#ifndef LOOP_H
#define LOOP_H

#include <functional>

class loop_predictor {
    static std::size_t constexpr INDEX_BITS = 6,
                          WAY_BITS = 2,
                          TAG_BITS = 14,
                          ITER_BITS = 14,
                          AGE_BITS = 8,
                          CONF_BITS = 2;

    static std::size_t constexpr tag_mask = (1 << TAG_BITS) - 1;

    static std::size_t constexpr PRED_SIZE = (1 << (INDEX_BITS + WAY_BITS)), WAY = (1 << WAY_BITS);

    std::array<uint16_t, PRED_SIZE> tag;
    std::array<uint16_t, PRED_SIZE> past_iter;
    std::array<uint16_t, PRED_SIZE> curr_iter;
    std::array<uint8_t, PRED_SIZE> age;
    std::array<uint8_t, PRED_SIZE> conf;

    bool valid;
    uint8_t seed;

    uint8_t last_pred;
    uint16_t last_tag;
    int entry_index, entry_way;

public:
    void init()
    {
        for (std::size_t i = 0; i < PRED_SIZE; ++i) {
            tag[i] = 0;
            past_iter[i] = 0;
            curr_iter[i] = 0;
            age[i] = 0;
            conf[i] = 0;
        }
        valid = false;
    }

    bool is_valid() const
    {
        return valid;
    }

    uint8_t predict(uint64_t ip)
    {
        entry_way = -1;
        entry_index = (ip & ((1 << INDEX_BITS) - 1)) << WAY_BITS;
        last_tag = std::hash<uint64_t>{}(ip) & ((1 << TAG_BITS) - 1);

        for (std::size_t i = entry_index; i < entry_index + WAY; ++i) {
            if (tag[i] == last_tag) {
                entry_way = i;
                valid = (conf[i] == 3);
                last_pred = ((curr_iter[i] + 1) == past_iter[i] ? 0 : 1);
                return last_pred;
            }
        }

        valid = false;
        last_pred = 0;
        return 0;
    }

    void update_state(uint64_t taken, uint64_t tage_pred)
    {
        if (entry_way >= 0) {
            int entry = entry_index + entry_way;
            if (valid) {
                if (taken != last_pred) {
                    age[entry] = 0;
                    past_iter[entry] = 0;
                    conf[entry] = 0;
                    curr_iter[entry] = 0;
                    return;
                }
                if (taken != tage_pred) {
                    if (age[entry] < 31)
                        ++age[entry];
                }
            }

            ++curr_iter[entry];
            curr_iter[entry] &= ((1 << ITER_BITS) - 1);

            if (curr_iter[entry] > past_iter[entry]) {
                conf[entry] = 0;
                if (past_iter[entry] != 0) {
                    past_iter[entry] = 0;
                    age[entry] = 0;
                    conf[entry] = 0;
                }
            }

            if (!taken) {
                if (curr_iter[entry] == past_iter[entry]) {
                    if (conf[entry] < 3)
                        ++conf[entry];

                    if (past_iter[entry] > 0 && past_iter[entry] < 3) {
                        past_iter[entry] = 0;
                        age[entry] = 0;
                        conf[entry] = 0;
                    }
                } else {
                    if (past_iter[entry] == 0) {
                        conf[entry] = 0;
                        past_iter[entry] = curr_iter[entry];
                    } else {
                        past_iter[entry] = 0;
                        age[entry] = 0;
                        conf[entry] = 0;
                    }
                }
                curr_iter[entry] = 0;
            }
        } else if (taken) {
            seed = (seed + 1) & ((1 << WAY_BITS) - 1);
            for (std::size_t i = 0; i < WAY; ++i) {
                int j = entry_index + ((seed + i) & ((1 << WAY_BITS) - 1));
                if (age[j] == 0) {
                    tag[j] = last_tag;
                    past_iter[j] = 0;
                    curr_iter[j] = 1;
                    conf[j] = 0;
                    age[j] = 31;
                    break;
                } else if (age[j] > 0)
                    --age[j];
            }
        }
    }
};

#endif // LOOP_H
