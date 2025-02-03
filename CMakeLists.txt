cmake_minimum_required(VERSION 3.22)
project(netlink2 C)

set(CMAKE_C_STANDARD 23)

add_executable(netlink2 misc/ss.c)
