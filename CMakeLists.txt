cmake_minimum_required (VERSION 3.0)
project(MDR VERSION 0.0.0.1)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_FLAGS "-g -O3")
set(CMAKE_VERBOSE_MAKEFILE ON)

set (MGARDx_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/external/MGARDx/install/include")

find_library(ZSTD_LIB zstd HINTS "/usr/lib")
find_library(SZ3_LIB SZ3 HINTS "${CMAKE_CURRENT_SOURCE_DIR}/external/SZ3/build/src")
set(ZSTD_INCLUDES "/usr/include")
set (SZ3_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/external/SZ3/include")

find_library(ADIOS2_LIB adios2_cxx11 HINTS "/usr/local/lib")
set (ADIOS2_INCLUDES "/usr/local/include")

find_library(ADIOS2MPI_LIB adios2_cxx11_mpi HINTS "/usr/local/lib")
set (ADIOS2MPI_INCLUDES "/usr/local/include")


find_library(EC_LIB erasurecode HINTS "/usr/local/lib")
set (EC_INCLUDES "/usr/local/include")

find_library(MPI_LIB mpi HINTS "/usr/local/lib")
set (MPI_INCLUDES "/usr/local/include")


find_library(ROCKSDB_LIB rocksdb HINTS "/usr/local/lib")
set (ROCKSDB_INCLUDES "/usr/local/include")


#find_library(BONMIN_LIB bonmin HINTS "/Users/lwk/Research/Projects/coin-or/bonmin/install/lib")
#set (BONMIN_INCLUDES "/Users/lwk/Research/Projects/coin-or/bonmin/install/include")

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)
#added lo tink libz
target_link_libraries(${PROJECT_NAME} INTERFACE z bz2 snappy lz4)  # Add 'snappy' and 'lz4'

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include)
add_subdirectory (test)
