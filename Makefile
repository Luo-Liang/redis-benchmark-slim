ALL:
	g++ -std=c++11 -O3 -g redis-benchmark.cc -lhiredis -o redis-benchmark 
