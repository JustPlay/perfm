#!/bin/sh

function p_msg()
{
    msg="$*"
    echo "$msg"
}

function p_nnl()
{
    msg="$*"
    echo -n "$msg"
}

function usage()
{
    p_msg "Usage: $(basename $0) <cpu-mask> <command> [params ...]"
    p_msg "       @cpu-mask@ is in the from: 0 | 0,2,5 | 0,2-5,7"
}

## Entry ##
if [ $# -le 2 ]; then
    usage
    exit 1
fi

TASKSET=$(which taskset)
AWK=$(which gawk)

CPUMASK=$1

COMMAND_A=""
for (( i = 2; i <= $#; ++i)); 
do    
    eval j=\$$i
    COMMAND_A="$COMMAND_A $j"
done
COMMAND_A=$(echo $COMMAND_A)

COMMAND_B=$(echo "$*" | $AWK '{ for (i = 2; i <= NF; ++i) printf $i" " }')


COMMAND_C=""
for (( i = 2; i <= $#; ++i)); 
do    
    j=${!i}
    COMMAND_C="$COMMAND_C $j"
done
COMMAND_C=$(echo $COMMAND_C)


p_msg ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
p_msg ">>> PID on CPUs: $$ => $CPUMASK"
p_msg ">>> CMD: $COMMAND_A"
p_msg ">>> CMD: $COMMAND_B"
p_msg ">>> CMD: $COMMAND_C"
p_msg ">>>"

$TASKSET -cp $CPUMASK $$

p_msg ">>>"
p_msg ">>> EXEC: $COMMAND_A"
p_msg ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"

exec $COMMAND_A
