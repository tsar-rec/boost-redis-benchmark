cmake_minimum_required(VERSION 3.22)
project(boost-redis-benchmark CXX)

set(DISTCC_GCC_MAJOR 12)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.84.0 REQUIRED COMPONENTS
    system
    )

set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

add_executable(batch_send_benchmark
        batch_send_benchmark.cpp
    )

target_link_libraries(batch_send_benchmark
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
    )
