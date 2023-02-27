#!/bin/bash

for SIZE in 200 250 300 350 400 450 500 550 600 650 700 750 800 850 900 950 1000 1100 1200 1300 1400 1500
do
	echo Running for $SIZE
	./ipbench --client=localhost --target=localhost --test=latency --test-args="bps=100000000,size=$SIZE"
	echo Done.
done
