cmake_minimum_required(VERSION 3.10)
project(secil)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -g -O0")

# We are using the nanopb library here
# NOTE: If you are already using nanopb in your project, you can skip this part
#       and just add the nanopb library to your project
add_library(nanopb
   lib/nanopb/pb_common.c
   lib/nanopb/pb_decode.c
   lib/nanopb/pb_encode.c
   )
target_include_directories(nanopb PUBLIC lib/nanopb)

add_library(secil 
   lib/secil.c
   lib/secil.pb.c
   )

target_link_libraries(secil nanopb)

# Include path for secil is only the /include directory
# All other includes are used internally by the library
target_include_directories(secil PUBLIC include)

