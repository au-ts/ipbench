# scripts

This directory includes several helpful scripts for getting started with ipbench.

## ununmarshall

Strips debugging output from latency test, in case you want to keep the results from a test run.


## runbench

This script is an example distributed tester script targeting the latency test. It is designed to target our cluster of testing machines at Trustworthy Systems (in `$TINNIES`). If you want to use this script, substitute `$TINNIES` and the code which appends `vb` to them to the ip address or local address of your tester machines.

The `nocpu` variant skips the CPU target portion of the test.


## ipbench_benchmark.sh

This is another benchmark used by TS. Similar to runbench.