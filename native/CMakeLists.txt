cmake_minimum_required(VERSION 3.22.1)
project(pgo_native C)

add_library(pgo_hook SHARED libpgo_hook.c)
set_target_properties(pgo_hook PROPERTIES OUTPUT_NAME pgo_hook)

# Diagnostic-only IL2CPP metadata probe, loaded only when its Morphe patch is selected.
add_library(pgo_rpc_probe SHARED libpgo_rpc_probe.c)
set_target_properties(pgo_rpc_probe PROPERTIES OUTPUT_NAME pgo_rpc_probe)

add_library(main_wrapper SHARED libmain_wrapper.c)
set_target_properties(main_wrapper PROPERTIES OUTPUT_NAME main)
# The stock Pokémon GO libmain.so has DT_SONAME "libmain.so". Since the
# patch renames that original file to libmain_orig.so, the wrapper must use
# a different SONAME; otherwise Android's linker can treat the renamed
# original as the already-loaded wrapper and recurse into JNI_OnLoad.
target_link_options(main_wrapper PRIVATE "-Wl,-soname,libmain_wrapper.so")

find_library(log-lib log)
find_library(dl-lib dl)
target_link_libraries(pgo_hook ${log-lib} ${dl-lib})
target_link_libraries(pgo_rpc_probe ${log-lib} ${dl-lib})
target_link_libraries(main_wrapper ${log-lib} ${dl-lib})
