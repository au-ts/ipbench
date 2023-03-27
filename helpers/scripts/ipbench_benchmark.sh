#!/bin/bash

CLIENTS="vb01 vb03 vb05 vb07"

TX2="10.13.1.74"
haswell3="172.16.1.50"
haswell4="10.13.1.73"
zynq7000="10.13.1.51"
sabre="10.13.1.137"
imx8mq="150.229.124.112"
imx8mm="172.16.1.85"

#TEST_TARGET=$haswell3
TEST_TARGET=$imx8mm
TARGET=$TEST_TARGET

THROUGHPUT_RANGE="10000000"
#THROUGHPUT_RANGE="20000000 40000000 60000000 80000000 100000000 120000000 150000000 180000000 200000000 210000000 220000000"
#THROUGHPUT_RANGE="100000000 200000000 300000000 400000000 500000000 600000000 700000000 800000000 900000000 1000000000"
#THROUGHPUT_RANGE="900000000 1000000000"

PACKET_SIZE_RANGE="1472"
#WARMUP_TIME=10
#COOLDOWN_TIME=10
SOCKTYPE="udp"
TARGET_PORT="1235"
#TARGET_PORT="7"
OUTPUT_FILE='output.csv'
#SAMPLES=200000
SAMPLES=20

for client in $CLIENTS
do
  CLIENT_ARGS=$CLIENT_ARGS" --client $client"
  NUM_CLIENTS=$((NUM_CLIENTS + 1))
done

echo "$CLIENT_ARGS"
echo "$NUM_CLIENTS"

echo "Requested_Throughput,Receive_Throughput,Send_Throughput,Packet_Size,Minimum_RTT,Average_RTT,Maximum_RTT,Stdev_RTT,Median_RTT,CPU_utilisation" > $OUTPUT_FILE

for throughput in $THROUGHPUT_RANGE
do
  for packetsize in $PACKET_SIZE_RANGE
  do
    echo Running $throughput bps, size $packetsize
    START_TIME=$SECONDS
    RESULT=`ipbench -d -p 8036 $CLIENT_ARGS --test-target="$TEST_TARGET" --test-port="$TARGET_PORT" --test="latency" --test-args="socktype=$SOCKTYPE,bps=$(($throughput/$NUM_CLIENTS)),size=$packetsize,warmup=$WARMUP_TIME,cooldown=$COOLDOWN_TIME,samples=$(($SAMPLES/$NUM_CLIENTS))" --target-test=cpu_target_lukem --target-test-hostname=$TEST_TARGET --target-test-port=1236`

    #RESULT=`ipbench -d -p 8036 $CLIENT_ARGS --test-target="$TEST_TARGET" --test-port="$TARGET_PORT" --test="discard" --test-args="bps=$(($throughput/$NUM_CLIENTS)),size=$packetsize,warmup=$WARMUP_TIME,cooldown=$COOLDOWN_TIME,test_time=20,ifname=eth0,proto=udp" --target-test=cpu_target_lukem --target-test-hostname=$TEST_TARGET --target-test-port=1236`

    echo $RESULT
    ELAPSED_TIME=$(($SECONDS - $START_TIME))
    echo $ELAPSED_TIME

    if [ "`echo $RESULT | grep '*'`" = "" ] ; then
    echo $RESULT   >> $OUTPUT_FILE
    else
    echo $RESULT >> $OUTPUT_FILE
    break;
    fi
    
    echo Done.
  done
done
