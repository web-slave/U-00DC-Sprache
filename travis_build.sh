#!/bin/bash

# Get llvm
wget http://releases.llvm.org/9.0.0/clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz &&\
xz -d clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz &&\
tar -xf clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04.tar &&\
\
# Get Boost
wget https://dl.bintray.com/boostorg/release/1.71.0/source/boost_1_71_0.tar.gz &&\
gzip -d boost_1_71_0.tar.gz &&\
tar -xf boost_1_71_0.tar &&\
\
# Configure build
mkdir build-travis &&\
cd build-travis &&\
cmake ../source/ -DCMAKE_BUILD_TYPE=Release -DBOOST_ROOT=../boost_1_71_0 -DLLVM_LIB_DIR=../clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04/lib/ &&\
\
# Build it
# travis-ci has 2 cpu cores
make -j 2  &&\
\
# Run tests
./Tests &&\
cd .. &&\
python3 source/annotated_tests_run.py --compiler-executable build-travis/Compiler --entry-point-source source/data/entry.cpp --use-position-independent-code --input-dir source/ustlib_test