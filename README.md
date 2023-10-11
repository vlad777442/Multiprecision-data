# Multi-precision data refactoring framework
Project: ESAMR (Enabling Scalable Analysis using Multi-precision Refactoring)<br />
Sponsor: ORNL LDRD<br />
Authors: Xin Liang, Jieyang Chen, Lipeng Wan, Ben Whitney, Qian Gong<br />
Supervisors: Scott Klasky, Rick Archibald<br />

# Installation
git clone https://github.com/lxAltria/Multiprecision-data-refactoring.git<br />
cd Multiprecision-data-refactoring<br />
./build_script.sh<br />

# Usage
cd build<br />
mkdir -p refactored_data<br />
Refactor: ./test/test_refactor $data_file $num_level $num_bitplanes $num_dims $dim0 $dim1 $dim2<br />
./test/test_refactor ../external/SZ3/data/Uf48.bin.dat 4 32 3 100 500 500<br />
Retrieval: ./test/test_retrieval $data_file $error_mode $error $s<br />
./test/test_reconstructor ../external/SZ3/data/Uf48.bin.dat 0 1.0 0<br />

# Notes and Parameters
During refactoring, the location of refactored data is hardcoded to "refactored_data/" directory under current directory. Need to create the directory before writing.<br />
data_file: path to input date file.<br />
num_levels: number of target decomposition levels.<br />
num_bitplanes: number of bitplanes for each level.<br />
num_dims: number of dimensions.<br />
Option: options of encoder/decomposer/retrieval etc. are changeable, but not supported in commandline for now (see these components in different folders of include and alter the options in test/test_refactor.cpp and test/test_reconstruct.cpp)<br />
error mode: error metric during retreival (see include/error_est.hpp)<br />
0: max error, i.e. L-infty<br />
1: squared error, i.e. L-2<br />
