# if(WIN32)
#   message("user/calice SiWECAL is not to be built on windows (fixme)")
#   return()
# endif()

cmake_dependent_option(USER_CALICE_BUILD_SiWECAL "build user/calice SiWECAL" ON
  "USER_CALICE_BUILD" OFF)

get_filename_component(EUDAQ_USERNAME ${CMAKE_CURRENT_LIST_DIR} NAME)
set(EUDAQ_MODULE module_${EUDAQ_USERNAME}) 

if(NOT USER_CALICE_BUILD_SiWECAL)
  message("user/calice SiWECAL is not to be built")
  return()
endif()

add_subdirectory(module)


