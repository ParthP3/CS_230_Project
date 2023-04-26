#include "ooo_cpu.h"
#include "../tage/tage.h"
#include "loop.h"

#include <map>

struct my_data {
    int loop_correct;
    uint8_t tage_pred;
    uint8_t loop_pred;
};

static std::map<O3_CPU*, tage_predictor<4>> Tage;
static std::map<O3_CPU*, loop_predictor> Loop;
static std::map<O3_CPU*, my_data> D;

void O3_CPU::initialize_branch_predictor()
{
    std::cout << "CPU " << cpu << " Loop-TageAGE branch predictor\n";
    Tage[this].init();
    Loop[this].init();
    D[this].loop_correct = 0;
}
uint8_t O3_CPU::predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type)
{
    if (always_taken)
        return 1;
    D[this].loop_pred = Loop[this].predict(ip);
    D[this].tage_pred = Loop[this].predict(ip);
    if (Loop[this].is_valid() && D[this].loop_correct >= 0)
        return D[this].loop_pred;
    return D[this].tage_pred;
}
void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
    Tage[this].update_state(ip, taken);
    Loop[this].update_state(taken, D[this].tage_pred);
    if (Loop[this].is_valid() && D[this].tage_pred != D[this].loop_pred) {
        if (taken == D[this].loop_pred) {
            if (D[this].loop_correct < 127)
                ++D[this].loop_correct;
        }
        else if (D[this].loop_correct > -126)
            --D[this].loop_correct;
    }
}
