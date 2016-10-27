#!/bin/bash

function pr_msg()
{
    msg="$*"
    echo "$msg"
}

function pr_nnl()
{
    msg="$*"
    echo -n "$*"
}

function pr_err()
{
    msg="$*"
    if [ -w /dev/stderr ]; then
        echo "$msg" >> /dev/stderr
    else
        echo "$msg"
    fi
}


if [ $EUID -ne 0 ]; then
    pr_msg "You need root privilege to do cpu freq scaling. the current cpu freq:"

    cat /proc/cpuinfo | grep -E '(MHz|processor)' | while read line;
    do
        processor=$(echo $line | grep 'processor')
        if [ -n "$processor" ]; then
            echo $processor | tr -d ' ' | gawk -F':' '{printf("- %s: #%-8d", $1, $2)}'
        fi 

        frequency=$(echo $line | grep 'cpu MHz')
        if [ -n "$frequency" ]; then
            echo $frequency | gawk -F':' '{printf("%.0fMHz\n", $2)}'
        fi
    done

    exit 0
fi


if [ $# -eq 2 ]; then
    SCALING_MIN_FREQ=$(echo "$1 * 1000000" | bc | gawk -F'.' '{print $1}')
    SCALING_MAX_FREQ=$(echo "$2 * 1000000" | bc | gawk -F'.' '{print $1}')
elif [ $# -eq 1 ]; then
    SCALING_MIN_FREQ=$(echo "$1 * 1000000" | bc | gawk -F'.' '{print $1}')
    SCALING_MAX_FREQ=$(echo "$1 * 1000000" | bc | gawk -F'.' '{print $1}')
else
    pr_err "Usage: $(basename $0) <min-freq-in-GHz (format: X.Y)> [max-freq-in-GHz (format: X.Y)]"
    exit 1
fi


if [ ${SCALING_MIN_FREQ} -gt ${SCALING_MAX_FREQ} ]; then
    pr_err "Invalid argrments, please ensuse 'min-freq <= max-freq'"
    exit 1;
fi


CPU_DIR="/sys/devices/system/cpu"
CPU_LST=$(cd ${CPU_DIR}; ls | grep -e "^cpu[[:digit:]]")


for cpu in ${CPU_LST} 
do
    pr_msg "[Processor $cpu]:"
    
    IF_ONLN=0
    if [ -f ${CPU_DIR}/${cpu}/online ] && [ -r ${CPU_DIR}/${cpu}/online ]; then
        IF_ONLN=$(cat ${CPU_DIR}/${cpu}/online)
    fi
   

    if [ "$IF_ONLN"X = "1"X ] || [ "${cpu}" = "cpu0" ]; then
        echo ${SCALING_MIN_FREQ} > ${CPU_DIR}/$cpu/cpufreq/scaling_min_freq
        echo ${SCALING_MAX_FREQ} > ${CPU_DIR}/$cpu/cpufreq/scaling_max_freq
        

        cpuinfo_cur_freq_new=$(cat ${CPU_DIR}/$cpu/cpufreq/cpuinfo_cur_freq)
        scaling_min_freq_new=$(cat ${CPU_DIR}/$cpu/cpufreq/scaling_min_freq)
        scaling_max_freq_new=$(cat ${CPU_DIR}/$cpu/cpufreq/scaling_max_freq)

        pr_msg "    scaling_min_freq: ${scaling_min_freq_new}"
        pr_msg "    scaling_max_freq: ${scaling_max_freq_new}"
        pr_msg "    cpuinfo_cur_freq: ${cpuinfo_cur_freq_new}"
    else
        pr_msg "    processor offline, skipping..."
    fi

    pr_msg ""
done
