#!/bin/bash
add_to_path ()
{
    if [[ "$PATH" =~ (^|:)"${1}"(:|$) ]]
    then
        return 0
    fi
    export PATH=${1}:$PATH
}
export TESTS_DPDK_HOME="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../dpdk/" && pwd )"
export RTE_SDK="$TESTS_DPDK_HOME/"
export RTE_TARGET="x86_64-native-linuxapp-gcc"
add_to_path "$TESTS_DPDK_HOME/usertools/"
