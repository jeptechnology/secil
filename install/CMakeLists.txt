cmake_minimum_required(VERSION 3.10)

project(secil)

add_library(secil 
   source/secil.c
   source/secil.pb.c
   source/pb_common.c
   source/pb_decode.c
   source/pb_encode.c
)

# Include path for secil is only the /include directory
# All other includes are used internally by the library
target_include_directories(secil 
   PUBLIC 
      include
   PRIVATE
      source
)