cmake_minimum_required(VERSION 3.16)
project(CppConcurrencyLabs LANGUAGES CXX)

# --- Shared settings for every lab ---------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

find_package(Threads REQUIRED)   # maps to -pthread on Linux, libc on QNX

# Helper: add_lab(<target> <source>) -> links pthreads + enables warnings.
function(add_lab target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE Threads::Threads)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

# Optional: build everything with ThreadSanitizer via -DENABLE_TSAN=ON
option(ENABLE_TSAN "Build with ThreadSanitizer" OFF)
if(ENABLE_TSAN)
  add_compile_options(-fsanitize=thread -g -O1)
  add_link_options(-fsanitize=thread)
endif()

# --- Labs -----------------------------------------------------------------
add_subdirectory(00_setup)
add_subdirectory(01_creating_threads)
add_subdirectory(02_passing_arguments)
add_subdirectory(03_thread_lifetime)
add_subdirectory(04_data_races)
add_subdirectory(05_mutex_raii)
add_subdirectory(06_deadlock)
add_subdirectory(07_condition_variables)
add_subdirectory(08_async_future_promise)
add_subdirectory(09_atomics)
add_subdirectory(10_thread_pool)
add_subdirectory(project_sensor_pipeline)
