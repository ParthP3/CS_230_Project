#include "ooo_cpu.h"
#include "tage.h"

#include <map>

static std::map<O3_CPU*, tage_predictor<4>> T;

void O3_CPU::initialize_branch_predictor()
{
    std::cout << "CPU " << cpu << " TAGE branch predictor\n";
    T[this].init();
}
uint8_t O3_CPU::predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type)
{
    if (always_taken)
        return 1;
    return T[this].predict(ip);
}
void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
    T[this].update_state(ip, taken);
}
