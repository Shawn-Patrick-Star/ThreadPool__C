cmake_minimum_required(VERSION 3.11)

project(ThreadPool)

find_package(Threads)

add_executable(ThreadPool Thread_pool.cc main.cc)

target_link_libraries(ThreadPool ${CMAKE_THREAD_LIBS_INIT})