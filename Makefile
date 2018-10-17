ALL:
	export RTE_SDK=~/dpdk/dpdk-stable
	export RTE_TARGET=x86_64-native-linuxapp-gcc
	export RTE_ANS=~/dpdk-ans
	export FF_PATH=~/f-stack
	export FF_DPDK=$RTE_SDK/$RTE_TARGET
 
	g++ -std=c++11 -O3 -g redis-benchmark.cc -lhiredis -o redis-benchmark 
	g++ -std=c++11 -DUSE_ANS_DPDK -O3 -g redis-benchmark.cc -L /home/ubuntu/dpdk-redis/deps/hiredis -lhiredis -o redis-benchmark-ans
