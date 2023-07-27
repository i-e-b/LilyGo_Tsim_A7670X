cmake_minimum_required(VERSION 3.26)

include(~/esp/esp-idf/tools/cmake/project.cmake)

set(CMAKE_C_COMPILER ~/.espressif/tools/xtensa-esp32-elf/esp-12.2.0_20230208/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc)
set(CMAKE_ASM_COMPILER ~/.espressif/tools/xtensa-esp32-elf/esp-12.2.0_20230208/xtensa-esp32-elf/bin/xtensa-esp32-elf-as)
set(CMAKE_CXX_COMPILER ~/.espressif/tools/xtensa-esp32-elf/esp-12.2.0_20230208/xtensa-esp32-elf/bin/xtensa-esp32-elf-g++)

include_directories("src")

project(07_espidf_test)
#add_library(07_espidf_test src/main.cpp)
#add_executable(EspIdf_Dbg src/main.cpp)
