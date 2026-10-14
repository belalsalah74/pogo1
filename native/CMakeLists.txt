cmake_minimum_required(VERSION 3.22.1)
project(pgo_native C)

add_library(pgo_hook SHARED libpgo_hook.c)
set_target_properties(pgo_hook PROPERTIES OUTPUT_NAME pgo_hook)

add_library(main_wrapper SHARED libmain_wrapper.c)
set_target_properties(main_wrapper PROPERTIES OUTPUT_NAME main)

find_library(log-lib log)
find_library(dl-lib dl)
target_link_libraries(pgo_hook ${log-lib} ${dl-lib})
target_link_libraries(main_wrapper ${dl-lib})
