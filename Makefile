
all: test remove-nonfull-blocks libFastPFor.a benchmark

libFastPFor.a:
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/bitpacking.cpp
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/bitpackingaligned.cpp
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/bitpackingunaligned.cpp
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/simdunalignedbitpacking.cpp
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/simdbitpacking.cpp
	ar rvs libFastPFor.a bitpacking.o bitpackingaligned.o bitpackingunaligned.o simdunalignedbitpacking.o simdbitpacking.o

benchmark: *.hpp *.h benchmark.cpp Makefile libFastPFor.a
	g++ -O3 -g  -msse4.2 -std=c++11 -Wall -o benchmark benchmark.cpp libFastPFor.a

test: test.cpp *.hpp Makefile libFastPFor.a
	g++ -O3 -g  -msse4.2 -std=c++11 -Wall -o test test.cpp libFastPFor.a

remove-nonfull-blocks: remove-nonfull-blocks.cpp *.hpp Makefile libFastPFor.a
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -o remove-nonfull-blocks remove-nonfull-blocks.cpp libFastPFor.a

clean:
	rm -f *.o *.a decode encode test benchmark remove-nonfull-blocks
