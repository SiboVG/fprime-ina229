set(SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/BenchClient.fpp" "${CMAKE_CURRENT_LIST_DIR}/BenchClient.cpp")
set(MOD_DEPS Ina229_Types)
register_fprime_module()
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/BenchClient.cpp" PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror;-pedantic")
