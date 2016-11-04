#!/bin/bash

set -u

TARGET="ev2perf"

if [ -f $TARGET ]; then
    rm -vf $TARGET
    echo ""
fi

SRC_FILE="perfm_util.cpp  perfm_pmu.cpp  perfm_option.cpp  perfm_event.cpp  ev2perf.cpp"

g++ -std=c++11 -g -Wall -lpfm $SRC_FILE -o $TARGET
