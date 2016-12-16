#!/bin/bash

set -u

TARGET="perfm"

if [ -f $TARGET ]; then
    rm -vf $TARGET
    echo ""
fi

SRC_FILE="perfm_util.cpp perfm_pmu.cpp perfm_option.cpp perfm_event.cpp perfm_group.cpp perfm_monitor.cpp perfm_analyzer.cpp perfm_top.cpp perfm_topology.cpp perfm.cpp"

g++ -std=c++11 -g -Wall -lpfm $SRC_FILE -o $TARGET -lrt -lncurses
