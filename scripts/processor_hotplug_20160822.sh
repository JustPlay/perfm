#!/bin/sh

### Util Functions ###
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


## Warning
pr_err "***"
pr_err "*** disable cpu cores"
pr_err "*** for now, this script can only be used on 2 sockets systems with ht disabled"
pr_err "***"


## Script entry
CPU_CFG_DIR="/sys/devices/system/cpu"

# Sockets installed on this system
NR_SYS_SKTS=2

# Threads per core
NR_THR_CORE=1

# Cores installed on this system
NR_SYS_CORE=$(ls $CPU_CFG_DIR | grep -E 'cpu[[:digit:]]' | wc -l)


if [ $# != 1 ] || [ $(($1 % $NR_SYS_SKTS)) -ne 0 ] ; then
    pr_err "Usage: $(basename $0) <cores to disable (must an multipler of sockets on this system)>"
    exit 1
fi


CORES_TO_DISABLE=$(expr $1 / $NR_SYS_SKTS)
CORES_PER_SOCKET=$(expr $NR_SYS_CORE / $NR_SYS_SKTS / $NR_THR_CORE)

#
# CORES_PER_SOCKET may *NOT* be computed by using the info from /proc/cpuinfo
#

if [ $CORES_TO_DISABLE -ge $CORES_PER_SOCKET ]; then
    pr_err "The specified core number are too big"
fi

## First, put all cores online
for ((i = 0; i < $NR_SYS_CORE; i++));
do
    if [ -f $CPU_CFG_DIR/"cpu"$i/online ] && [ -w $CPU_CFG_DIR/"cpu"$i/online ]; then
        echo 1 > $CPU_CFG_DIR/"cpu"$i/online
    fi
done


pr_msg "------------------------------------------------------------"
pr_msg "Sockets on this system:   $NR_SYS_SKTS"
pr_msg "Cores   on this system:   $NR_SYS_CORE"
pr_msg "Cores   on each socket:   $CORES_PER_SOCKET"
pr_msg ""
pr_msg "Total  cores to disable:  $1"
pr_msg "Socket cores to disable:  $CORES_TO_DISABLE"
pr_msg "------------------------------------------------------------"



sleep 5


## Second, disable some cores
for ((i = 1; i <= $CORES_TO_DISABLE; i++)); 
do
    CPU_ID=$(expr $CORES_PER_SOCKET - $i)
    pr_msg "--  disable core: $CPU_ID"
    echo 0 > $CPU_CFG_DIR/"cpu"$CPU_ID/"online"

    CPU_ID=$(expr $CPU_ID + $CORES_PER_SOCKET)
    pr_msg "--  disable core: $CPU_ID"
    echo 0 > $CPU_CFG_DIR/"cpu"$CPU_ID/"online"
done

## Final, display current cores info
NR_CORE_LINE=$(cat /proc/cpuinfo | grep 'processor' | wc -l)
pr_msg ""
pr_msg "Current online cores: $NR_CORE_LINE"
cat /proc/cpuinfo | grep 'processor' |  tr -d '\t' | gawk -F':' '{printf("--  %s:%s\n", $1, $2)}'


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
