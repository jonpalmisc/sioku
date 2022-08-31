cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(Sioku LANGUAGES C)

add_library(Sioku SHARED Sioku.h Sioku.c)
target_compile_features(Sioku PRIVATE c_std_99)
target_include_directories(Sioku PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Sioku PUBLIC "-framework CoreFoundation -framework IOKit")
