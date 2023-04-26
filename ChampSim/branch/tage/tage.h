#ifndef TAGE_H
#define TAGE_H

#include <bitset>
#include <cmath>
#include <functional>
#include <utility>

#include <cstdint>

inline void hash_combine(std::size_t& seed)
{
}

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    hash_combine(seed, rest...);
}

std::size_t constexpr L(std::size_t i)
{
    float constexpr alpha = 1.5;
    std::size_t constexpr L0 = 2;
    return (size_t)(std::pow(alpha, i) * L0 + 0.5);
}

template <std::size_t NUM_COMPONENTS>
class tage_predictor {
    static std::size_t constexpr PRED_SIZE = 1024,
                                 TAG_BITS = 8,
                                 COUNTER_BITS = 6,
                                 USEFUL_BITS = 2,
                                 USEFUL_RESET_PERIOD = 18;

    static std::size_t constexpr tag_mask = (1 << TAG_BITS) - 1;

    // two hash functions
    static std::size_t index_hash(uint64_t const& ip, std::bitset<L(NUM_COMPONENTS - 1)> const& b)
    {
        std::size_t ret = 0;
        hash_combine(ret, ip, b);
        return ret;
    }
    static std::size_t tag_hash(uint64_t const& ip, std::bitset<L(NUM_COMPONENTS - 1)> const& b)
    {
        std::size_t ret = 0xdcc9797bb583d4;
        hash_combine(ret, ip, b);
        return ret;
    }

    // incrementing and decrementing counters and useful
    static void incc(int& m)
    {
        m = std::min(m + 1, (1 << COUNTER_BITS) - 1);
    }
    static void incu(int& u)
    {
        u = std::min(u + 1, (1 << USEFUL_BITS) - 1);
    }
    static void dec(int& m)
    {
        m = std::max(m - 1, 0);
    }

    std::size_t nr_branches = 0;
    bool msb_reset = true;

    std::bitset<L(NUM_COMPONENTS - 1)> ght;
    std::bitset<L(NUM_COMPONENTS - 1)> ght_p(std::size_t n)
    {
        std::size_t z = L(NUM_COMPONENTS - 1) - n;
        return (ght << z) >> z;
    }

    // base predictor counter
    std::array<int, PRED_SIZE> counter_base;

    // tagged predictor tags, counters and useful bits
    std::array<std::array<uint8_t, PRED_SIZE>, NUM_COMPONENTS> tag;
    std::array<std::array<int, PRED_SIZE>, NUM_COMPONENTS> counter_tagged, useful;

    // the last prediction given
    uint8_t last_pred;

    // provider and altpred components for the last prediction
    // provider, altpred < NUM_COMPONENTS ==> tagged
    // provider, altpred = NUM_COMPONENTS ==> base
    std::size_t provider, altpred;

public:
    void init()
    {
        uint16_t start_counter = (1 << (COUNTER_BITS - 1));
        for (std::size_t j = 0; j < PRED_SIZE; ++j)
            counter_base[j] = start_counter;
        for (std::size_t i = 0; i < NUM_COMPONENTS; ++i) {
            for (std::size_t j = 0; j < PRED_SIZE; ++j)
                counter_tagged[i][j] = start_counter;
            for (std::size_t j = 0; j < PRED_SIZE; ++j)
                tag[i][j] = 0;
            for (std::size_t j = 0; j < PRED_SIZE; ++j)
                useful[i][j] = 0;
        }
        last_pred = 0;
        provider = altpred = NUM_COMPONENTS;
        srand(0);
    }

    uint8_t predict(uint64_t ip)
    {
        ++nr_branches;

        bool found_provider = false;
        altpred = provider = NUM_COMPONENTS;

        // checking for tagged component hit
        for (int c = NUM_COMPONENTS - 1; c >= 0; --c) {
            std::size_t i = index_hash(ip, ght_p(L(c))) % PRED_SIZE;
            if (tag[c][i] == (tag_hash(ip, ght_p(L(c))) & tag_mask)) {
                if (!found_provider) {
                    provider = c;
                    last_pred = counter_tagged[c][i] >> (COUNTER_BITS - 1);
                    found_provider = true;
                } else {
                    altpred = c;
                    break;
                }
            }
        }
        if (found_provider)
            return last_pred;

        std::size_t i_base = std::hash<uint64_t>{}(ip) % PRED_SIZE;
        last_pred = counter_base[i_base] >> (COUNTER_BITS - 1);
        return last_pred;
    }

    void update_state(uint64_t ip, uint64_t taken)
    {
        // graceful_reset
        if ((nr_branches >> USEFUL_BITS) > 0) {
            int k = (msb_reset ? 1: 2);
            msb_reset = !msb_reset;
            for (std::size_t c = 0; c < NUM_COMPONENTS; ++c)
                for (std::size_t i = 0; i < PRED_SIZE; ++i)
                    useful[c][i] &= k;
            nr_branches = 0;
        }

        std::size_t i_provider = index_hash(ip, ght_p(L(provider))) % PRED_SIZE;
        std::size_t i_altpred = index_hash(ip, ght_p(L(altpred))) % PRED_SIZE;
        std::size_t i_base = std::hash<uint64_t>{}(ip) % PRED_SIZE;

        // updating u and inserting entries if possible
        if (taken == last_pred) {
            if (provider != NUM_COMPONENTS && altpred != NUM_COMPONENTS
                    && counter_tagged[provider][i_provider] >> (COUNTER_BITS - 1) != counter_tagged[altpred][i_altpred] >> (COUNTER_BITS - 1) )
                incu(useful[provider][i_provider]);
        } else {
            std::size_t s = (provider != NUM_COMPONENTS) ? (provider + 1): 0;

            std::size_t j = NUM_COMPONENTS, k = NUM_COMPONENTS;
            for (std::size_t c = s; c < NUM_COMPONENTS; ++c) {
                std::size_t index = index_hash(ip, ght_p(L(c))) % PRED_SIZE;
                if (useful[c][index] == 0) {
                    if (j == NUM_COMPONENTS)
                        j = c;
                    else if (k == NUM_COMPONENTS) {
                        k = c;
                        break;
                    }
                }
            }
            if (j == NUM_COMPONENTS) {
                // all decremented
                for (std::size_t c = s; c < NUM_COMPONENTS; ++c) {
                    std::size_t index = index_hash(ip, ght_p(L(c))) % PRED_SIZE;
                    dec(useful[c][index]);
                }
            } else if (k == NUM_COMPONENTS) {
                // entry into j
                std::size_t index = index_hash(ip, ght_p(L(j))) % PRED_SIZE;
                counter_tagged[j][index] = (1 << (COUNTER_BITS - 1)) - (taken ? 0 : 1);
                useful[j][index] = 0;
                tag[j][index] = tag_hash(ip, ght_p(L(j))) & tag_mask;
            } else {
                // probability business
                if (rand() * 3 < RAND_MAX)
                    j = k;
                std::size_t index = index_hash(ip, ght_p(L(j))) % PRED_SIZE;
                counter_tagged[j][index] = (1 << (COUNTER_BITS - 1)) - (taken ? 0 : 1);
                useful[j][index] = 0;
                tag[j][index] = tag_hash(ip, ght_p(L(j))) & tag_mask;
            }

            if (provider != NUM_COMPONENTS && altpred != NUM_COMPONENTS
                    && counter_tagged[provider][i_provider] >> (COUNTER_BITS - 1) != counter_tagged[altpred][i_altpred] >> (COUNTER_BITS - 1))
                dec(useful[provider][i_provider]);
        }

        // updating counter counter
        if (provider == NUM_COMPONENTS) {
            if (taken == 1)
                incc(counter_base[i_base]);
            else
                dec(counter_base[i_base]);
        } else {
            if (taken == 1)
                incc(counter_tagged[provider][i_provider]);
            else
                dec(counter_tagged[provider][i_provider]);
        }

        ght <<= 1;
        ght.set(0, taken);
    }
};

#endif // TAGE_H
