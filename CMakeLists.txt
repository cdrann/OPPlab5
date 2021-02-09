cmake_minimum_required(VERSION 3.14)
project(OPPlab5)

set(CMAKE_CXX_STANDARD 14)

add_executable(OPPlab5 lab5.cpp)

# Require MPI for this project:
find_package(MPI REQUIRED)
set(CMAKE_CXX_COMPILE_FLAGS ${CMAKE_CXX_COMPILE_FLAGS} ${MPI_COMPILE_FLAGS})
set(CMAKE_CXX_LINK_FLAGS ${CMAKE_CXX_LINK_FLAGS} ${MPI_LINK_FLAGS})
include_directories(${MPI_INCLUDE_PATH})
target_link_libraries(OPPlab5 ${MPI_LIBRARIES})

#link to pthread library
find_package(Threads REQUIRED)
if(THREADS_HAVE_PTHREAD_ARG)
    set_property(TARGET OPPlab5 PROPERTY COMPILE_OPTIONS "-pthread")
    set_property(TARGET OPPlab5 PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(OPPlab5 "${CMAKE_THREAD_LIBS_INIT}")
endif()