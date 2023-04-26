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
std::size_t index_hash(uint64_t const& ip, std::bitset<L(NUM_COMPONENTS - 1)> const& b)
{
    std::size_t ret = 0;
    hash_combine(ret, ip, b);
    return ret;
}
std::size_t tag_hash(uint64_t const& ip, std::bitset<L(NUM_COMPONENTS - 1)> const& b)
{
    std::size_t ret = 0xdcc9797bb583d4;
    hash_combine(ret, ip, b);
    return ret;
}

std::size_t constexpr PRED_SIZE = 1024;
std::size_t constexpr COUNTER_BITS = 3;
std::size_t constexpr USEFUL_BITS = 2;
std::size_t constexpr USEFUL_RESET_PERIOD = 18;

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

static std::size_t nr_branches = 0;
static bool msb_reset = true;

using hist_t = std::bitset<L(NUM_COMPONENTS - 1)>;
static std::map<O3_CPU*, hist_t> ght;
inline hist_t ght_p(O3_CPU* p, std::size_t n)
{
    std::size_t z = L(NUM_COMPONENTS - 1) - n;
    return (ght[p] << z) >> z;
}

static std::map<O3_CPU*, std::array<int, PRED_SIZE>> mode_base;

static std::map<O3_CPU*, std::array<std::array<uint64_t, PRED_SIZE>, NUM_COMPONENTS>> tag;
static std::map<O3_CPU*, std::array<std::array<int, PRED_SIZE>, NUM_COMPONENTS>> mode_tagged;
static std::map<O3_CPU*, std::array<std::array<int, PRED_SIZE>, NUM_COMPONENTS>> useful;

static uint8_t last_pred;
static std::size_t provider, altpred;
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
    srand(0);
}

uint8_t O3_CPU::predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type)
{
    ++nr_branches;

    bool found_provider = false;
    altpred = provider = NUM_COMPONENTS;

    // checking for tagged component hit
    for (int c = NUM_COMPONENTS - 1; c >= 0; --c) {
        std::size_t i = index_hash(ip, ght_p(this, L(c))) % PRED_SIZE;
        if (tag[this][c][i] == (tag_hash(ip, ght_p(this, L(c))) & 0xff)) {
            if (!found_provider) {
                provider = c;
                last_pred = mode_tagged[this][c][i] >> (COUNTER_BITS - 1);
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
    last_pred = mode_base[this][i_base] >> (COUNTER_BITS - 1);
    return last_pred;
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
    // graceful_reset
    if ((nr_branches >> USEFUL_BITS) > 0) {
        int k = (msb_reset ? 1: 2);
        msb_reset = !msb_reset;
        for (std::size_t c = 0; c < NUM_COMPONENTS; ++c)
            for (std::size_t i = 0; i < PRED_SIZE; ++i)
                useful[this][c][i] &= k;
        nr_branches = 0;
    }

    std::size_t i_provider = index_hash(ip, ght_p(this, L(provider))) % PRED_SIZE;
    std::size_t i_altpred = index_hash(ip, ght_p(this, L(altpred))) % PRED_SIZE;
    std::size_t i_base = std::hash<uint64_t>{}(ip) % PRED_SIZE;

    // updating u and inserting entries if possible
    if (taken == last_pred) {
        if (provider != NUM_COMPONENTS && altpred != NUM_COMPONENTS
                && mode_tagged[this][provider][i_provider] >> (COUNTER_BITS - 1) != mode_tagged[this][altpred][i_altpred] >> (COUNTER_BITS - 1) )
            incu(useful[this][provider][i_provider]);
    } else {
        std::size_t s = (provider != NUM_COMPONENTS) ? (provider + 1): 0;

        std::size_t j = NUM_COMPONENTS, k = NUM_COMPONENTS;
        for (std::size_t c = s; c < NUM_COMPONENTS; ++c) {
            std::size_t index = index_hash(ip, ght_p(this, L(c))) % PRED_SIZE;
            if (useful[this][c][index] == 0) {
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
                std::size_t index = index_hash(ip, ght_p(this, L(c))) % PRED_SIZE;
                dec(useful[this][c][index]);
            }
        } else if (k == NUM_COMPONENTS) {
            // entry into j
            std::size_t index = index_hash(ip, ght_p(this, L(j))) % PRED_SIZE;
            mode_tagged[this][j][index] = (1 << (COUNTER_BITS - 1)) - (taken ? 0 : 1);
            useful[this][j][index] = 0;
            tag[this][j][index] = tag_hash(ip, ght_p(this, L(j))) & 0xff;
        } else {
            // probability business
            if (rand() * 3 < RAND_MAX)
                j = k;
            std::size_t index = index_hash(ip, ght_p(this, L(j))) % PRED_SIZE;
            mode_tagged[this][j][index] = (1 << (COUNTER_BITS - 1)) - (taken ? 0 : 1);
            useful[this][j][index] = 0;
            tag[this][j][index] = tag_hash(ip, ght_p(this, L(j))) & 0xff;
        }

        if (provider != NUM_COMPONENTS && altpred != NUM_COMPONENTS
                && mode_tagged[this][provider][i_provider] >> (COUNTER_BITS - 1) != mode_tagged[this][altpred][i_altpred] >> (COUNTER_BITS - 1))
            dec(useful[this][provider][i_provider]);
    }

    // updating mode counter
    if (provider == NUM_COMPONENTS) {
        if (taken == 1)
            incc(mode_base[this][i_base]);
        else
            dec(mode_base[this][i_base]);
    } else {
        if (taken == 1)
            incc(mode_tagged[this][provider][i_provider]);
        else
            dec(mode_tagged[this][provider][i_provider]);
    }

    ght[this] <<= 1;
    ght[this].set(0, taken);
}
