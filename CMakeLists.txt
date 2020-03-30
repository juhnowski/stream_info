cmake_minimum_required(VERSION 3.15)
project(stream_info)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(AVCODEC_LIBRARY avcodec)
find_library(AVFORMAT_LIBRARY avformat)
find_library(AVUTIL_LIBRARY avutil)
find_library(SWSCALE_LIBRARY swscale)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(stream_info main.cpp)

target_link_libraries(stream_info ${OpenCV_LIBS} ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY})

configure_file(test.sdp test.sdp COPYONLY)