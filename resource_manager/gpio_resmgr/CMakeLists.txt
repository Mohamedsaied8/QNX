cmake_minimum_required(VERSION 3.24)
project(gpio_resmgr VERSION 0.1.0 LANGUAGES C)

# do not append any suffix as we are targeting QNX
set(CMAKE_EXECUTABLE_SUFFIX "")

add_executable(gpio_resmgr gpio_resmgr.c)
add_executable(gpio_client gpio_client.c)

foreach(tgt gpio_resmgr gpio_client)
  target_compile_features(${tgt} PRIVATE c_std_11)
  target_compile_options(${tgt} PRIVATE -Wall -Wextra)
  target_include_directories(${tgt} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

# QNX_PROCESSOR is inherited from the QNX toolchain file
install(TARGETS gpio_resmgr gpio_client DESTINATION nto/${QNX_PROCESSOR}/bin)
