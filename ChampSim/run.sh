#!/bin/bash

bin/champsim --warmup_instructions 20000000 --simulation_instructions 30000000 ../traces/cc-14.trace.gz > ../results_20_30/cc/$1
bin/champsim --warmup_instructions 20000000 --simulation_instructions 30000000 ../traces/sssp-14.trace.gz > ../results_20_30/sssp/$1