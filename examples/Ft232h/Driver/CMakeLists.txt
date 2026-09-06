set(SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/BridgeDriver.fpp"
 "${CMAKE_CURRENT_LIST_DIR}/BridgeDriver.cpp" "${CMAKE_CURRENT_LIST_DIR}/../UnixSpi.cpp")
set(MOD_DEPS Ina229_Device Ina229_Types)
register_fprime_module()
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/BridgeDriver.cpp" "${CMAKE_CURRENT_LIST_DIR}/../UnixSpi.cpp"
 PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror;-pedantic")
