### Branch Predictors in Graph Analytics
# Team Name
# Team Members
 - Anish Yogesh Kulkarni (21b090003)
 - Parth Pujari (210100106)
 - Arnav Aditya Singh (210050018)
## Branch Prediction using TAGE
We added the TAGE branch predictor ([`branch/tage`](branch/tage)) as a part of our project. It is fully configureable with regards to bit sizes, number of components etc.
We have also implemented the L-TAGE branch predictor ([`branch/l_tage`](branch/l_tage)), but unfortunaley have not been able to iron out the bugs.

## Compile and Run
First modify `champsim_config.json` to use the desired branch predictor (`tage` or `l_tage`).

    $ ./config.sh champsim_config.json
    $ make -j$(nproc)
    $ bin/champsim --warmup_instructions <no_of_warmup_instr> --simulation_instructions <no_of_sim_instr> <path _to_trace_file>

For our results ([`results`](results)) we used 30000000 instructions for both warmup and simulation.
