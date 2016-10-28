#!/bin/bash

grep 'processor'   /proc/cpuinfo | gawk -F':' '{printf "%-3d", $2}' | tr -d '\n' | gawk '{printf("processor:   %s\n", $0)}' 
grep 'core id'     /proc/cpuinfo | gawk -F':' '{printf "%-3d", $2}' | tr -d '\n' | gawk '{printf("core id:     %s\n", $0)}' 
grep 'physical id' /proc/cpuinfo | gawk -F':' '{printf "%-3d", $2}' | tr -d '\n' | gawk '{printf("physical id: %s\n", $0)}' 
