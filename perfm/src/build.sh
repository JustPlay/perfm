#!/bin/bash

if [ -f "perfm" ]; then
    rm -vf perfm

    echo ""
fi

g++ *.cpp -std=c++11 -g -Wall -lpfm -o perfm
