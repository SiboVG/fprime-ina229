add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Top")
set(SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/Main.cpp")
set(MOD_DEPS Deployment_Top Os)
register_fprime_deployment()

set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/Main.cpp" PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror;-pedantic")
