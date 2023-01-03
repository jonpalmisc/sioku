cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(sioku LANGUAGES C)

add_library(sioku STATIC sioku.h sioku.c)
target_compile_features(sioku PRIVATE c_std_99)
target_include_directories(sioku PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sioku PUBLIC "-framework CoreFoundation -framework IOKit")
