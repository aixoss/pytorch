#!/usr/bin/env bash
cd third_party
yes | rm -r googletest >/dev/null 2>&1
yes | rm -r protobuf >/dev/null 2>&1
git clone https://github.com/protocolbuffers/protobuf.git -b v3.6.0
git clone https://github.com/kavanabhat/googletest.git -b master
cd ..
export CXXFLAGS="-mvsx -maix64"
export CFLAGS="-mvsx -maix64"
export CXX='g++ -L/opt/freeware/lib/pthread/ppc64 -lstdc++ -pthread'
export LDFLAGS='-latomic -lpthread -Wl,-bbigtoc'
export CC="gcc -L/opt/freeware/lib/pthread/ppc64 -pthread -maix64 -mvsx"
ulimit -d unlimited
