#!/bin/bash

bin/champsim --warmup_instructions 30000000 --simulation_instructions 30000000 ../traces/cc-14.trace.gz > ../results/cc/$1 &
bin/champsim --warmup_instructions 30000000 --simulation_instructions 30000000 ../traces/sssp-14.trace.gz > ../results/sssp/$1 &
bin/champsim --warmup_instructions 30000000 --simulation_instructions 30000000 ../traces/bc-5.trace.gz > ../results/bc/$1 &
bin/champsim --warmup_instructions 30000000 --simulation_instructions 30000000 ../traces/bfs-10.trace.gz > ../results/bfs/$1 &
