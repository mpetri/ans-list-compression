
all: test.x remove-nonfull-blocks.x libFastPFor.a benchmark.x

libFastPFor.a:
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/bitpacking.cpp
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/bitpackingaligned.cpp
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/bitpackingunaligned.cpp
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/simdunalignedbitpacking.cpp
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/simdbitpacking.cpp
	ar rvs libFastPFor.a bitpacking.o bitpackingaligned.o bitpackingunaligned.o simdunalignedbitpacking.o simdbitpacking.o

benchmark_cikm.x: *.hpp *.h benchmark_cikm.cpp Makefile libFastPFor.a
	g++ -O3 -g  -msse4.2 -std=c++11 -Wall -o benchmark_cikm.x benchmark_cikm.cpp libFastPFor.a

test.x: test.cpp *.hpp Makefile libFastPFor.a
	g++ -O3 -g  -msse4.2 -std=c++11 -Wall -o test.x test.cpp libFastPFor.a

remove-nonfull-blocks.x: remove-nonfull-blocks.cpp *.hpp Makefile libFastPFor.a
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -o remove-nonfull-blocks.x remove-nonfull-blocks.cpp libFastPFor.a

clean:
	rm -f *.o *.a test.x benchmark_cikm.x remove-nonfull-blocks.x
