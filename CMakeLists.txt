cmake_minimum_required(VERSION 3.10)
project(secil)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -g -O0")

# include the schema folder to build the protobuf files
# This will generate the secil.pb.c and secil.pb.h files
# The generated files will be placed in the lib folder
add_subdirectory(schema)

add_library(secil 
   source/secil.c
)

target_link_libraries(secil schema)

# Include path for secil is only the /include directory
# All other includes are used internally by the library
target_include_directories(secil PUBLIC include)

# Loopback Test
add_executable(loopback_test
   test/test_loopback.c
   example/common.c
)
target_include_directories(loopback_test PUBLIC example)

target_link_libraries(loopback_test secil)

# Linux UART Test
add_executable(se_example
   example/se_example.c
   example/common.c)   
target_link_libraries(se_example secil)

add_executable(eme_example
   example/eme_example.c
   example/common.c)   
target_link_libraries(eme_example secil)

# Now we set up some install rules so that we can emplace the library and headers in the correct locations
set(secil_install_dir "${CMAKE_CURRENT_BINARY_DIR}/install")

# We want to create a directory structure like this:
# install/
# ├── CMakeLists.txt
# ├── include
# │   └── secil.h
# └── source
#     ├── secil.pb.h
#     ├── secil.pb.c
#     ├── pb.h
#     ├── pb_common.h
#     ├── pb_common.c
#     ├── pb_decode.h
#     ├── pb_decode.c
#     ├── pb_encode.h
#     ├── pb_encode.c
#     └── secil.c

install(
   FILES 
      install/CMakeLists.txt
   DESTINATION ${secil_install_dir}
)

install(
   FILES 
      include/secil.h
   DESTINATION ${secil_install_dir}/include
)

install(
   FILES 
      source/secil.c
      ${CMAKE_BINARY_DIR}/schema/secil.pb.h
      ${CMAKE_BINARY_DIR}/schema/secil.pb.c
      nanopb/pb.h
      nanopb/pb_common.h
      nanopb/pb_common.c
      nanopb/pb_decode.h
      nanopb/pb_decode.c
      nanopb/pb_encode.h
      nanopb/pb_encode.c
   DESTINATION ${secil_install_dir}/source
)
