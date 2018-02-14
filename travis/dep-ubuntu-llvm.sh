#!/bin/sh

cat ./travis/llvm-snapshot.gpg.key | sudo apt-key add -
echo "deb http://apt.llvm.org/trusty/ llvm-toolchain-$(lsb_release -cs)-$LLVM_VER main" | sudo tee /etc/apt/sources.list.d/llvm.list
