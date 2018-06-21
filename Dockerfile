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
ADD . /code
WORKDIR /code
RUN make V8_VERSION="branch-heads/6.8" v8-checkout && make -j v8
RUN mkdir build && cd build && cmake .. && make
