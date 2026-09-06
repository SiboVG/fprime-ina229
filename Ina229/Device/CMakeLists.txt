set(SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/Device.cpp")
register_fprime_module()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(Ina229_Device PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
