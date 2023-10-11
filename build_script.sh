#!/bin/bash

source_dir=`pwd`
external_dir=${source_dir}/external
mkdir -p external
cd ${external_dir}
# build SZ (to use ZSTD compressor)
git clone https://github.com/szcompressor/SZ.git
cd SZ
git reset --hard f48d2f27a5470a28e900db9b46bb3344a2bc211f
mkdir -p build
mkdir -p install
cd build
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_INSTALL_PREFIX=${external_dir}/SZ/install ..
make -j 8
make install

# build SZ3 (to use quantizer and huffman encoder)
cd ${external_dir}
git clone https://github.com/szcompressor/SZ3.git
cd SZ3
#git reset --hard de980d9ce85dd3bec7856ab9007abe5d3c2e262f
git reset --hard 7fa420d4fc6b0ffa9365ce9b1f815546c3cc125a
cp ${source_dir}/SZ3_CMakeLists.txt CMakeLists.txt
mkdir -p build
cd build
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..
make -j 8

# build MGARDx
cd ${external_dir}
git clone https://github.com/lxAltria/MGARDx.git
cd MGARDx
#git reset --hard 28d738c3a533db1943bfef7717587a7ac4c93b5f
mkdir -p build
mkdir -p install
cd build
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_INSTALL_PREFIX=${external_dir}/MGARDx/install ..
make -j 8
make install

# clone compression utils
cd ${external_dir}
git clone https://github.com/lxAltria/compression_utils.git

# build MDR
cd ${source_dir}
mkdir -p build
cd build
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..
make -j 8
