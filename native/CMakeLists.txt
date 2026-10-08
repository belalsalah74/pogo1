cmake_minimum_required(VERSION 3.22.1)
project(pgo_native C)

add_library(main SHARED libmain_wrapper.c)
target_link_libraries(main PRIVATE dl)
set_target_properties(main PROPERTIES OUTPUT_NAME "main")

add_library(pgo_hook SHARED libpgo_hook.c)
