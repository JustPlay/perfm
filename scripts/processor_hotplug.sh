#!/bin/sh

function pr_msg()
{
    MSG="$*"
    echo "$MSG"
}

function pr_nnl()
{
    MSG="$*"
    echo -n "$*"
}

function pr_err()
{
    MSG="$*"
    if [ -w /dev/stderr ]; then
        echo "$MSG" >> /dev/stderr
    else
        echo "$MSG"
    fi
}


function pr_warning()
{
    pr_err "Warning:"
    pr_err "    (1) This script is used to disable *CORES* not *THREADS*"
    pr_err "    (2) This script will disable the same numbers of cores on each socket"
    pr_err "    (3) *BE CAREFUL* with the [Threads per core] argument"
}

function pr_usage()
{
    pr_warning
    pr_msg ""

    pr_err "Usage:"
    pr_err "    $(basename $0) <Cores to disable> <Threads per core> [Sockets (defaults to 2)]"
}


CPU_CFG_DIR="/sys/devices/system/cpu"
NR_THR_CORE=  # Threads per core
NR_SYS_SKTS=  # Sockets installed on this system

## Script entry
case $# in
3)
    NR_THR_CORE=$2 # Threads per core
    NR_SYS_SKTS=$3 # Sockets installed on this system
;;

2)
    NR_THR_CORE=$2 # Threads per core
    NR_SYS_SKTS=2  # Sockets installed on this system (Assumed 2 sockets)
;;

*)
    pr_usage
    exit 1
esac


if [ $(($1 % $NR_SYS_SKTS)) -ne 0 ] ; then
    pr_err "Invalid argument: the first argument must be an multipler of sockets on this system"
    exit 1
fi

# Thrds installed on this system
NR_SYS_THRD=$(ls $CPU_CFG_DIR | grep -E 'cpu[[:digit:]]' | wc -l)

# Cores installed on this system
NR_SYS_CORE=$(($NR_SYS_THRD / $NR_THR_CORE))

# Cores to disable on each socket
CORES_TO_DISABLE=$(expr $1 / $NR_SYS_SKTS)

# Cores per socket
CORES_PER_SOCKET=$(expr $NR_SYS_CORE / $NR_SYS_SKTS)

#
# CORES_PER_SOCKET may *NOT* be computed by using the info from /proc/cpuinfo
#

if [ $CORES_TO_DISABLE -ge $CORES_PER_SOCKET ]; then
    pr_err "Invalid argument: the specified core number are too big"
    exit 1
fi

## First, put all cores/thrds online
for ((i = 0; i < $NR_SYS_THRD; i++));
do
    if [ -f $CPU_CFG_DIR/"cpu"$i/online ] && [ -w $CPU_CFG_DIR/"cpu"$i/online ]; then
        echo 1 > $CPU_CFG_DIR/"cpu"$i/online
    fi
done


pr_msg "------------------------------------------------------------"
pr_msg "Sockets on this system:     $NR_SYS_SKTS"
pr_msg "Cores   on this system:     $NR_SYS_CORE"
pr_msg "Thrds   on this system:     $NR_SYS_THRD"
pr_msg ""
pr_msg "Cores   on each socket:     $CORES_PER_SOCKET"
pr_msg "Thrds   on each socket:     $(($CORES_PER_SOCKET * $NR_THR_CORE))"
pr_msg ""
pr_msg "Total  *cores* to disable:  $1" 
pr_msg "Total  *thrds* to disable:  $(($1 * $NR_THR_CORE))"
pr_msg ""
pr_msg "Socket *cores* to disable:  $CORES_TO_DISABLE"
pr_msg "Socket *thrds* to disable:  $(($CORES_TO_DISABLE * $NR_THR_CORE))"

sleep 2

NR_PROCESSOR_ONLINE=$(cat /proc/cpuinfo | grep 'processor' | wc -l)
if [ $NR_PROCESSOR_ONLINE -ne $NR_SYS_THRD ]; then
    pr_err "Fault Error, Exiting..."
    exit 1
fi


## Second, disable the same *cores* on each socket
pr_msg "------------------------------------------------------------"
for ((c = 1; c <= $CORES_TO_DISABLE; c++)); 
do
    for ((s = 1; s <= $NR_SYS_SKTS; s++));
    do
        # The last online core on each socket
        CORE_ID=$(($CORES_PER_SOCKET * $s - $c))
        
        # index0, L1 data cache
        # index1, L1 code cache
        # index2, L2 unified cache
        # index3, L3 unified cache
        SHARED_CPU_LIST=$(cat ${CPU_CFG_DIR}/cpu${CORE_ID}/cache/index0/shared_cpu_list | tr ',' ' ' )
        
        for p in $SHARED_CPU_LIST;
        do
            pr_msg "--  unplug core:thrd: ${CORE_ID}:${p}"
            echo 0 > $CPU_CFG_DIR/"cpu"$p/"online"
        done
    done
done

sleep 2

## Final, display current cores/thrds info

NR_PROCESSOR_ONLINE=$(cat /proc/cpuinfo | grep 'processor' | wc -l)
pr_msg "------------------------------------------------------------"
pr_msg "Current online processors: $NR_PROCESSOR_ONLINE"
cat /proc/cpuinfo | grep 'processor' |  tr -d '\t' | gawk -F':' '{printf("--  %s:%s\n", $1, $2)}'

pr_msg "------------------------------------------------------------"
./processor_map.sh

## Processors mapping on E5-2640-V4 without HT
#
#  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19
#  0  1  2  3  4  8  9 10 11 12  0  1  2  3  4  8  9 10 11 12
#  0  0  0  0  0  0  0  0  0  0  1  1  1  1  1  1  1  1  1  1


## Processors mapping on E5-2640-V4 with HT
#
#  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39
#  0  1  2  3  4  8  9 10 11 12  0  1  2  3  4  8  9 10 11 12  0  1  2  3  4  8  9 10 11 12  0  1  2  3  4  8  9 10 11 12
#  0  0  0  0  0  0  0  0  0  0  1  1  1  1  1  1  1  1  1  1  0  0  0  0  0  0  0  0  0  0  1  1  1  1  1  1  1  1  1  1
#
##
#
# echo "turn off the cores,input the start and end"
# echo 0 > /sys/devices/system/cpu/cpu$1/online
# echo $1
# $tmp1=expr $1 + 10 
# $tmp2=expr $1 + 20 
# $tmp3=expr $1 + 30 
# echo 0 > /sys/devices/system/cpu/cpu$tmp1/online
# echo 0 > /sys/devices/system/cpu/cpu$tmp2/online
# echo 0 > /sys/devices/system/cpu/cpu$tmp3/online
#
##
