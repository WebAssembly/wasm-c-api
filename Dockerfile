FROM ubuntu:bionic
RUN apt-get update && apt-get install -y \
    apt-utils \
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
RUN make v8-checkout && make -j v8
