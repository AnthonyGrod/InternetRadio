cmake_minimum_required (VERSION 3.1)

project(SIKRADIO)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -O2")

FIND_PACKAGE( Boost COMPONENTS program_options REQUIRED )

add_executable(sikradio-receiver sikradio-receiver.cpp common.h err.h)
add_executable(sikradio-sender sikradio-sender.cpp common.h err.h)
add_executable(lookup lookup.cpp common.h err.h)
add_executable(uidemo uidemo.cpp)
target_link_libraries(sikradio-receiver pthread)
target_link_libraries(sikradio-sender pthread)
target_link_libraries(sikradio-receiver Boost::program_options)
target_link_libraries(sikradio-sender Boost::program_options)
