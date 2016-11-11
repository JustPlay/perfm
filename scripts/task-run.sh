#!/bin/bash
#
# copyright (c) 2016 justplay (hy). all rights Reserved
#
# author:
#     hy
#
# date:
#     2016/11/08
#
# brief:
#     todo
#
# globals:
#     CPU_DEVICES 
#
# arguments:
#
#     
#
# returns:
#     succ: 0
#     fail: 1


# set -u          # 使用的变量必须提前定义过
# set -o nounset  # 使用的变量必须提前定义过

# set -e          # 所有非0的返回状态都需要捕获
# set -o errexit  # 所有非0的返回状态都需要捕获

# set -o pipefail # 管道间错误需要捕获

# set -x          #
# set -o xtrace   #

set -u


declare -r CPU_DEVICES="/sys/devices/system/cpu"
declare -r MSRTOOL_DIR="msrtool"
declare -r TASKCFG_FLE="task.cfg"


function pr_msg()
{
    local msg="$*"
    echo "$msg"
}

function pr_nnl()
{
    local msg="$*"
    echo -n "$msg"
}

function pr_err()
{
    local msg="$*"
    if [ -w /dev/stderr ]; then
        echo "$msg" >> /dev/stderr
    else
        echo "$msg"
    fi
}

function usage()
{
    pr_err "Usage:"
    pr_err "    $(basename $0) <smt/core> <sockets> [interval]"
}

# unplug the *same* number of *core* on each socket
#
# $1 - # of cores to unplug
# $2 - # of smt per core
# $3 - # of sockets installed on this system
function processor_hotplug()
{
    local nr_smt_core=$2
    local nr_sys_skts=$3

    if [ $nr_sys_skts -le 0 ] || [ $nr_smt_core -le 0 ] || [ $(($1 % $nr_sys_skts)) -ne 0 ]; then
        pr_err "invalid argument: the first argument must be an multipler of sockets on this system"
        exit 1
    fi

    # thrds installed on this system
    local nr_sys_thrd=$(ls $CPU_DEVICES | grep 'cpu[0-9]\{1,\}' | wc -l)

    # cores installed on this system
    local nr_sys_core=$(($nr_sys_thrd / $nr_smt_core))

    # cores per socket
    local nr_core_socket=$(($nr_sys_core / $nr_sys_skts))

    # cores to unplug on each socket
    local nr_core_unplug=$(($1 / $nr_sys_skts))

    if [ $nr_core_unplug -ge $nr_core_socket ]; then
        pr_err "invalid argument: the specified core number are too big"
        exit 1
    fi

    # first, put all cores/thrds online
    for ((i = 0; i < $nr_sys_thrd; i++));
    do
        if [ -f $CPU_DEVICES/"cpu"$i/online ] && [ -w $CPU_DEVICES/"cpu"$i/online ]; then
            echo 1 > $CPU_DEVICES/"cpu"$i/online
        fi
    done

    pr_msg "system: $nr_sys_skts sockets, $nr_sys_core cores, $nr_sys_thrd threads"
    pr_msg "unplug: $1 cores, $nr_core_unplug cores/socket"

    local nr_cpu_onln=$(cat /proc/cpuinfo | grep 'processor' | wc -l)
    if [ $nr_cpu_onln -ne $nr_sys_thrd ]; then
        pr_err "fault error: can *not* put all core/thread online, exiting..."
        exit 1
    fi

    ## second, disable the same *cores* on each socket
    for ((c = 1; c <= $nr_core_unplug; c++)); 
    do
        for ((s = 1; s <= $nr_sys_skts; s++));
        do
            # the last online core on each socket
            local core_id=$(($nr_core_socket * $s - $c))
            
            # index0, L1 data cache
            # index1, L1 code cache
            # index2, L2 unified cache
            # index3, L3 unified cache
            local cpu_shared_list=$(cat ${CPU_DEVICES}/cpu${core_id}/cache/index0/shared_cpu_list | tr ',' ' ' )
            for p in $cpu_shared_list;
            do
                echo 0 > $CPU_DEVICES/"cpu"$p/"online"
            done
        done
    done

    ## finally, do some checks
    nr_cpu_onln=$(cat /proc/cpuinfo | grep 'processor' | wc -l)
    if [ $nr_cpu_onln -ne $(($nr_sys_thrd - $nr_core_unplug * $nr_sys_skts * $nr_smt_core)) ]; then
        pr_err "fault error: unplug cores failed, exiting..."
        exit 1
    fi
}

function processor_freq_scaling()
{
    local cpu_min_freq=$(echo "$1 * 1000000" | bc | gawk -F'.' '{print $1}')
    local cpu_max_freq=$(echo "$2 * 1000000" | bc | gawk -F'.' '{print $1}')

    if [ ${cpu_min_freq} -gt ${cpu_max_freq} ]; then
        pr_err "invalid argrments: please ensure ..."
        exit 1;
    fi


    # set cpu frequency scaling
    local cpu_list=$(ls $CPU_DEVICES | grep 'cpu[0-9]\{1,\}')

    for cpu in ${cpu_list} 
    do
        local if_onln=0
        if [ -f ${CPU_DEVICES}/${cpu}/online ] && [ -r ${CPU_DEVICES}/${cpu}/online ]; then
            if_onln=$(cat ${CPU_DEVICES}/${cpu}/online)
        fi
       
        if [ "$if_onln"X = "1"X ] || [ "${cpu}" = "cpu0" ]; then
            echo ${cpu_min_freq} > ${CPU_DEVICES}/$cpu/cpufreq/scaling_min_freq
            echo ${cpu_max_freq} > ${CPU_DEVICES}/$cpu/cpufreq/scaling_max_freq
        fi
    done

    # finally, do some checks
    local cpu_freq_list=$(cat /proc/cpuinfo | grep -i 'mhz' | gawk -F':' '{print $2}' | tr -d ' ')
    for freq in ${cpu_freq_list}
    do
        local cpu_cur_freq=$(echo "scale=2; $freq * 1000" | bc | gawk -F'.' '{printf $1}')
        if [ $cpu_cur_freq -lt $cpu_min_freq ] || [ $cpu_cur_freq -gt $cpu_max_freq ]; then
            pr_err "fault error: set cpu freq failed, exiting..."
            exit 1
        fi
    done
}

function processor_llc_scaling()
{
    #firstly, set llc cache ways
    local llc_div=$(($1 / 4))
    local llc_mod=$(($1 % 4))

    local llc_way=""
    for ((i = 0; i < $llc_div; ++i));
    do
        llc_way="f"$llc_way
    done

    case $llc_mod in
        3)
            llc_way="7"$llc_way
        ;;
        2)
            llc_way="3"$llc_way
        ;;
        1)
            llc_way="1"$llc_way
        ;;

        *)
        ;;
    esac

    llc_way="0x"$llc_way

    local ret=0

    $MSRTOOL_DIR/wrmsr -a 0xc8f 0
    ret=$? 

    $MSRTOOL_DIR/wrmsr -a 0xc90 $llc_way
    ret=$(($ret + $?))

    if [ $ret -ne 0 ]; then
        pr_err "fault error: set llc cache ways failed, exiting..."
        exit 1
    fi

    # finally, do some checks
    local llc_way_list=$($MSRTOOL_DIR/rdmsr -a 0xc90)
    for cur_llc_way in ${llc_way_list}
    do
        if [ "0x$cur_llc_way" != "$llc_way" ]; then
            pr_err "fault error: set llc cache ways failed, exiting..."
            exit 1
        fi
    done
}

function execute()
{
    return 0   
}

function entry()
{
    local nr_smt_core=$1  # smt/core
    local nr_sys_skts=$2  # sockets installed on this system
    local tv_interval=$3  # interval time value in seconds

    # thrds installed on this system
    local nr_sys_thrd=$(ls $CPU_DEVICES | grep 'cpu[0-9]\{1,\}' | wc -l)

    # cores installed on this system
    local nr_sys_core=$(($nr_sys_thrd / $nr_smt_core))

    # cpu min freq
    local cpu_min_freq=1.2

    while read line;
    do
        if [ -z "$line" ]; then
            continue
        fi

        local nr_fields=$(echo $line | gawk -F' ' '{ print NF }'); # number of fields
        if [ $nr_fields -lt 3 ]; then
            pr_err "invalid line format, ignored: $line"
            continue
        fi

        local field_1th=$(echo $line | gawk -F' ' '{ print $1 }'); # nr_core
        local field_2th=$(echo $line | gawk -F' ' '{ print $2 }'); # frequency
        local field_3th=$(echo $line | gawk -F' ' '{ print $3 }'); # llc ways
        local field_xth=$(echo $line | gawk '{ for (i = 4; i <= NF; ++i) printf $i" " }')

        local cpu_onln=$field_1th
        local cpu_freq=$field_2th
        local llc_ways=$field_3th
        local cmd_exec=$field_xth

        # firstly, put all core/smt online
        processor_hotplug      0  $nr_smt_core  $nr_sys_skts > /dev/null

        # secondly, do some settings
        processor_llc_scaling  $llc_ways
        processor_freq_scaling $cpu_min_freq  $cpu_freq
        processor_hotplug      $(($nr_sys_core - $cpu_onln))  $nr_smt_core  $nr_sys_skts

        # finally, execute the specified command
        local skt_onln=$nr_sys_skts
        local smt_onln=$(($cpu_onln * $nr_smt_core))
        local local_id=$(date +%F-%T)
        local hwcfg_id="${skt_onln}s${cpu_onln}c${smt_onln}t_${local_id}"
        $cmd_exec  $hwcfg_id
    done < <(cat $TASKCFG_FLE)
}


if [ $EUID -ne 0 ]; then
    pr_err "$(basename $0): you need root privilege to run this script"
    exit 1
fi

case $# in
3)
    cd $(dirname $0) && entry $1 $2 $3
;;

2)
    cd $(dirname $0) && entry $1 $2 10
;;

*)
    usage
    exit 1
esac
