
all: encode decode

encode: encode.cpp *.hpp
	g++ -O3 -msse4.2 -std=c++11 -Wall -o encode encode.cpp compress_qmx.cpp

decode: decode.cpp *.hpp Makefile
	g++ -O3 -msse4.2 -std=c++11 -Wall -o decode decode.cpp compress_qmx.cpp