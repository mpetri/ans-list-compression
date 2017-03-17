
all: test encode decode

test: test.cpp *.hpp Makefile
	g++ -O3 -g  -msse4.2 -std=c++11 -Wall -o test test.cpp compress_qmx.cpp

encode: encode.cpp *.hpp
	g++ -O3 -g -msse4.2 -std=c++11 -Wall -o encode encode.cpp compress_qmx.cpp

decode: decode.cpp *.hpp Makefile
	g++ -O0 -g -msse4.2 -std=c++11 -Wall -o decode decode.cpp compress_qmx.cpp