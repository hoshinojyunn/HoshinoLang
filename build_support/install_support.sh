#!/bin/bash

sudo apt-add-repository "deb http://apt.llvm.org/focal/ llvm-toolchain-focal main"

sudo apt update

sudo apt install -y clang-14

llvm_version=$(clang-14 --version | grep "clang version" | cut -d " " -f 4)
echo "LLVM version: $llvm_version"
