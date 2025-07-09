cmake_minimum_required(VERSION 3.13.1)
set(EXTRA_ZEPHYR_MODULES ${CMAKE_CURRENT_LIST_DIR}/../../../)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_ble_manikin_decode_test)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})