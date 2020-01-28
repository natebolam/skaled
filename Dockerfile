FROM ubuntu:bionic
RUN mkdir /src
WORKDIR /src
COPY . /src

ENV CC gcc-7
ENV CXX g++-7
ENV TARGET all
ENV TRAVIS_BUILD_TYPE Debug
ENV CMAKE_BUILD_TYPE Debug

RUN apt-get update
RUN apt-get install -yq  libprocps-dev g++-7 valgrind gawk sed libffi-dev ccache libgoogle-perftools-dev \
    flex bison yasm texinfo autotools-dev automake python python-pip \
    cmake libtool build-essential pkg-config autoconf wget git libjsoncpp-dev  libargtable2-dev \
    libcurl4-openssl-dev libmicrohttpd-dev libhiredis-dev redis-server openssl lcov clang-format-6.0 libprocps-dev

RUN ./SkaleDeps/build.sh

RUN cmake -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE

RUN mkdir -p build; cd build; cmake -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE

### RUN cd libBLS; cmake -H. -Bbuild; cmake --build build -- -j$(nproc);

ENTRYPOINT ["/bin/bash"]