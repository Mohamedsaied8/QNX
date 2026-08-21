cmake_minimum_required(VERSION 3.24)
project(rpi4_resmgr_client VERSION 0.1.0 LANGUAGES C)

# do not append any suffix as we are targeting QNX
set(CMAKE_EXECUTABLE_SUFFIX "")

add_executable(rpi4_resmgr rpi4_resmgr.c)
add_executable(rpi4_client rpi4_client.c)

foreach(tgt rpi4_resmgr rpi4_client)
  target_compile_features(${tgt} PRIVATE c_std_11)
  target_compile_options(${tgt} PRIVATE -Wall -Wextra)
  target_include_directories(${tgt} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

# QNX_PROCESSOR is inherited from the QNX toolchain file
install(TARGETS rpi4_resmgr rpi4_client DESTINATION nto/${QNX_PROCESSOR}/bin)
