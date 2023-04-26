#include "ooo_cpu.h"
#include "../tage/tage.h"

#include <map>

static std::map<O3_CPU*, tage_predictor<4>> T;

void O3_CPU::initialize_branch_predictor()
{
    std::cout << "CPU " << cpu << " L-TAGE branch predictor\n";
    T[this].init();
}
uint8_t O3_CPU::predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type)
{
    return T[this].predict(ip, always_taken);
}
void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
    T[this].update_state(ip, taken);
}
