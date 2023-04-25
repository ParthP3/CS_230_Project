#include <bitset>
#include <cmath>
#include <functional>
#include <map>
#include <utility>

#include <cstdint>

#include "ooo_cpu.h"

// number of tagged components
std::size_t constexpr NUM_COMPONENTS = 4;

std::size_t constexpr L(int i)
{
    float constexpr alpha = 2;
    std::size_t constexpr L1 = 2;
    return (size_t)(std::pow(alpha, i - 1) * L1 + 0.5);
}

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
std::size_t my_hash(uint64_t const& ip, std::bitset<L(NUM_COMPONENTS - 1)> const& b)
{
    std::size_t ret = 0;
    hash_combine(ret, ip, b);
    return ret;
}

std::size_t constexpr PRED_SIZE = 16384;
std::size_t constexpr COUNTER_BITS = 3;
std::size_t constexpr USEFUL_BITS = 3;

inline void incc(int& m)
{
    m = std::min(m + 1, (1 << COUNTER_BITS) - 1);
}
inline void incu(int& u)
{
    u = std::min(u + 1, (1 << USEFUL_BITS) - 1);
}
inline void dec(int& m)
{
    m = std::max(m - 1, 0);
}

std::map<O3_CPU*, std::bitset<L(NUM_COMPONENTS - 1)>> ght;

std::map<O3_CPU*, std::array<int, PRED_SIZE>> mode_base;

std::map<O3_CPU*, std::array<std::array<uint64_t, PRED_SIZE>, NUM_COMPONENTS>> tag;
std::map<O3_CPU*, std::array<std::array<int, PRED_SIZE>, NUM_COMPONENTS>> mode_tagged;
std::map<O3_CPU*, std::array<std::array<int, PRED_SIZE>, NUM_COMPONENTS>> useful;

uint8_t last_pred;
std::size_t provider, altpred;

// provider, altpred < NUM_COMPONENTS ==> tagged
// provider, altpred = NUM_COMPONENTS ==> base

void O3_CPU::initialize_branch_predictor()
{
    std::cout << "CPU " << cpu << " TAGE branch predictor\n";

    uint16_t start_mode = (1 << (COUNTER_BITS - 1));
    for (std::size_t j = 0; j < PRED_SIZE; ++j)
        mode_base[this][j] = start_mode;
    for (std::size_t i = 0; i < NUM_COMPONENTS; ++i) {
        for (std::size_t j = 0; j < PRED_SIZE; ++j)
            mode_tagged[this][i][j] = start_mode;
        for (std::size_t j = 0; j < PRED_SIZE; ++j)
            tag[this][i][j] = 0;
        for (std::size_t j = 0; j < PRED_SIZE; ++j)
            useful[this][i][j] = 0;
    }
}

uint8_t O3_CPU::predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type)
{
    bool found = false;
    for (int c = NUM_COMPONENTS - 1; c >= 0; --c) {
        std::size_t i = my_hash(ip, ght[this] >> (L(NUM_COMPONENTS - 1) - L(c))) % PRED_SIZE;
        if (tag[this][c][i] == ip && useful[this][c][i] > 0) {
            if (!found) {
                provider = c;
                last_pred = mode_tagged[this][c][i] >> (COUNTER_BITS - 1);
                found = true;
            } else {
                altpred = c;
                return last_pred;
            }
        }
    }
    std::size_t i = std::hash<uint64_t>{}(ip) % PRED_SIZE;
    altpred = provider = NUM_COMPONENTS;
    last_pred = mode_base[this][i] >= (1 << (COUNTER_BITS - 1));
    return last_pred;
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
    ght[this] <<= 1;
    ght[this].set(0, taken);
    if (taken == last_pred) {
        if (provider != NUM_COMPONENTS) {
            std::size_t i_provider = my_hash(ip, ght[this] >> (L(NUM_COMPONENTS - 1) - L(provider))) % PRED_SIZE;
            std::size_t i_altpred = my_hash(ip, ght[this] >> (L(NUM_COMPONENTS - 1) - L(altpred))) % PRED_SIZE;
            if (mode_tagged[this][provider][i_provider] >> (COUNTER_BITS - 1) != mode_tagged[this][altpred][i_altpred] >> (COUNTER_BITS - 1))
                incu(useful[this][provider][i_provider]);
            incc(mode_tagged[this][provider][i_provider]);
        }
    } else {
        if (provider != NUM_COMPONENTS - 1) {
            std::size_t j = NUM_COMPONENTS, k = NUM_COMPONENTS;
            for (std::size_t l = provider + 1; l < NUM_COMPONENTS; ++l) {
                std::size_t index = my_hash(ip, ght[this] >> (L(NUM_COMPONENTS - 1) - L(l))) % PRED_SIZE;
                if (useful[this][l][index] == 0) {
                    if (j == NUM_COMPONENTS) {
                        j = l;
                    } else if (k == NUM_COMPONENTS) {
                        k = l;
                        break;
                    }
                }
            }
            if (j == NUM_COMPONENTS) {
                // all decremented
                for (std::size_t c = 0; c < NUM_COMPONENTS; ++c) {
                    std::size_t index = my_hash(ip, ght[this] >> (L(NUM_COMPONENTS - 1) - L(c))) % PRED_SIZE;
                    dec(useful[this][c][index]);
                }
            } else if (k == NUM_COMPONENTS) {
                // entry into j
                std::size_t index = my_hash(ip, ght[this] >> (L(NUM_COMPONENTS - 1) - L(j))) % PRED_SIZE;
                mode_tagged[this][j][index] = (1 << (COUNTER_BITS - 1));
                useful[this][j][index] = 0;
                tag[this][j][index] = ip;
            } else {
                // probability business
                srand(0);
                if (rand() * 3 < RAND_MAX)
                    j = k;
                std::size_t index = my_hash(ip, ght[this] >> (L(NUM_COMPONENTS - 1) - L(j))) % PRED_SIZE;
                mode_tagged[this][j][index] = (1 << (COUNTER_BITS - 1));
                useful[this][j][index] = 0;
                tag[this][j][index] = ip;
            }
        }
    }
}
