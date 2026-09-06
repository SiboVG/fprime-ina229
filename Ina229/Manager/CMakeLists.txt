set(SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/Manager.fpp" "${CMAKE_CURRENT_LIST_DIR}/Manager.cpp")
set(MOD_DEPS Ina229_Device Ina229_Types Os)
register_fprime_module()
set(UT_SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/Manager.fpp"
 "${CMAKE_CURRENT_LIST_DIR}/test/ut/ManagerTester.cpp"
 "${CMAKE_CURRENT_LIST_DIR}/test/ut/ManagerTestMain.cpp")
set(UT_MOD_DEPS Ina229_Device Ina229_Types STest)
set(UT_AUTO_HELPERS ON)
register_fprime_ut()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/Manager.cpp"
        PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror;-pedantic")
endif()
