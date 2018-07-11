FROM ubuntu:bionic
RUN apt-get update && apt-get install -y \
    apt-utils \
    clang \
    cmake \
    curl \
    git \
    libc++-dev \
    libc++abi-dev \
    libglib2.0-dev \
    libgmp-dev \
    ninja-build \
    python
ADD . /code/wasm-c-api
WORKDIR /code/wasm-c-api
RUN make v8-checkout && make -j v8
RUN make C_COMP=gcc LD_FLAGS= C_FLAGS= 
