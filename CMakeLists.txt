cmake_minimum_required (VERSION 3.2.2)
project (ans-compression CXX)

find_package(Boost COMPONENTS program_options filesystem REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2" )

include_directories(${CMAKE_SOURCE_DIR}/FastPFor-master/headers/)

add_library(FastPFor ./FastPFor-master/src/bitpacking.cpp
                     ./FastPFor-master/src/bitpackingaligned.cpp
                     ./FastPFor-master/src/bitpackingunaligned.cpp
                     ./FastPFor-master/src/simdunalignedbitpacking.cpp
                     ./FastPFor-master/src/simdbitpacking.cpp)


# OUR BINS
add_executable(benchmark.x benchmark.cpp)
target_link_libraries(benchmark.x FastPFor ${Boost_LIBRARIES})

add_executable(benchmark_cikm.x benchmark_cikm.cpp)
target_link_libraries(benchmark_cikm.x FastPFor ${Boost_LIBRARIES})


add_executable(test.x test.cpp)
target_link_libraries(test.x FastPFor ${Boost_LIBRARIES})



