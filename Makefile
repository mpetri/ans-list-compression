
all: test.x remove-nonfull-blocks.x libFastPFor.a benchmark.x

libFastPFor.a:
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/bitpacking.cpp
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/bitpackingaligned.cpp
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/bitpackingunaligned.cpp
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/simdunalignedbitpacking.cpp
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -I FastPFor-master/headers/ -c FastPFor-master/src/simdbitpacking.cpp
	ar rvs libFastPFor.a bitpacking.o bitpackingaligned.o bitpackingunaligned.o simdunalignedbitpacking.o simdbitpacking.o

benchmark.x: *.hpp *.h benchmark.cpp Makefile libFastPFor.a
	g++ -O3 -g  -msse4.2 -std=c++11 -Wall -o benchmark.x benchmark.cpp libFastPFor.a

test.x: test.cpp *.hpp Makefile libFastPFor.a
	g++ -O3 -g  -msse4.2 -std=c++11 -Wall -o test.x test.cpp libFastPFor.a

remove-nonfull-blocks.x: remove-nonfull-blocks.cpp *.hpp Makefile libFastPFor.a
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -o remove-nonfull-blocks.x remove-nonfull-blocks.cpp libFastPFor.a

clean:
	rm -f *.o *.a test.x benchmark.x remove-nonfull-blocks.x

bsmall: benchmark.x
	./benchmark.x ./freqs 2501 2500 < /mnt/d/list-freqs.txt

bfreqs: benchmark.x
	./benchmark.x ./freqs < /mnt/d/list-freqs.txt

bgaps: benchmark.x
	./benchmark.x ./gaps < /mnt/d/list-gaps.txt