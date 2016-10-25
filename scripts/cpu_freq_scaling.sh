#!/bin/bash

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


if [ $# -eq 2 ]; then
    SCALING_MIN_FREQ=$(echo "$1 * 1000000" | bc | gawk -F'.' '{print $1}')
    SCALING_MAX_FREQ=$(echo "$2 * 1000000" | bc | gawk -F'.' '{print $1}')
elif [ $# -eq 1 ]; then
    SCALING_MIN_FREQ=$(echo "$1 * 1000000" | bc | gawk -F'.' '{print $1}')
    SCALING_MAX_FREQ=$(echo "$1 * 1000000" | bc | gawk -F'.' '{print $1}')
else
    pr_err "Usage: $(basename $0) <scaling_min_freq(format: X.Y)> [scaling_max_freq(format: X.Y)]"
    exit 1
fi

if [ ${SCALING_MIN_FREQ} -gt ${SCALING_MAX_FREQ} ]; then
    pr_err "Invalid Argrments, Please ensuse [scaling_min_freq <= scaling_max_freq]"
    exit 1;
fi


CPU_DIR="/sys/devices/system/cpu"
CPU_LST=$(cd ${CPU_DIR}; ls |grep -e "^cpu[[:digit:]]")


pr_msg "-------------------------------------"

for cpu in ${CPU_LST}
do

    pr_msg "[Processor $cpu]:"
    
    IF_ONLINE=0
    if [ -f ${CPU_DIR}/${cpu}/online ] && [ -r ${CPU_DIR}/${cpu}/online ]; then
        IF_ONLINE=$(cat ${CPU_DIR}/${cpu}/online)
    fi
   

    if [ "$IF_ONLINE"X = "1"X ] || [ "${cpu}" = "cpu0" ]; then
        CPUINFO_CUR_FREQ_OLD=$(cat ${CPU_DIR}/$cpu/cpufreq/cpuinfo_cur_freq)
        SCALING_MIN_FREQ_OLD=$(cat ${CPU_DIR}/$cpu/cpufreq/scaling_min_freq)
        SCALING_MAX_FREQ_OLD=$(cat ${CPU_DIR}/$cpu/cpufreq/scaling_max_freq)

        echo ${SCALING_MIN_FREQ} > ${CPU_DIR}/$cpu/cpufreq/scaling_min_freq
        echo ${SCALING_MAX_FREQ} > ${CPU_DIR}/$cpu/cpufreq/scaling_max_freq
        

        CPUINFO_CUR_FREQ_NEW=$(cat ${CPU_DIR}/$cpu/cpufreq/cpuinfo_cur_freq)
        SCALING_MIN_FREQ_NEW=$(cat ${CPU_DIR}/$cpu/cpufreq/scaling_min_freq)
        SCALING_MAX_FREQ_NEW=$(cat ${CPU_DIR}/$cpu/cpufreq/scaling_max_freq)

        pr_msg "    scaling_min_freq: ${SCALING_MIN_FREQ_OLD} ---> ${SCALING_MIN_FREQ_NEW}"
        pr_msg "    scaling_max_freq: ${SCALING_MAX_FREQ_OLD} ---> ${SCALING_MAX_FREQ_NEW}"
        pr_msg "    cpuinfo_cur_freq: ${CPUINFO_CUR_FREQ_OLD} ---> ${CPUINFO_CUR_FREQ_NEW}"
    else
        pr_msg "    [今天我休息]"
    fi

    pr_msg ""
done

pr_msg "-------------------------------------"
