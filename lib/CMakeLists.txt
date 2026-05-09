# libx6100js8 — JS8 protocol engine library for the X6100.
#
# Phase 3+: JS8Call-interoperable info-bit layout via lib/js8encode/.
#   * fate (MIT)        — LDPC encoder, recode, fsk modulator (lib pipeline tail)
#   * jsc  (GPL-3)      — Word-dictionary compression (Phase 3 body frames)
#   * js8encode (GPL-3) — JS8Call-layout 87-bit info vector + frame builders
#   * src  (GPL-3)      — Public C API
#
# Mixed licenses force the combined library to GPL-3-or-later.

cmake_minimum_required(VERSION 3.18)
project(x6100js8 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(x6100js8 SHARED
    src/x6100js8.cc
    js8encode/js8encode.cpp
    js8encode/frames.cpp
    js8encode/directed.cpp
    js8encode/data.cpp
    ../target/fate/pack.cc
    ../target/fate/libldpc.cc
    ../target/fate/util.cc
    ../target/fate/unpack.cc
    ../target/src/recode.cc
    jsc/jsc.cpp
    jsc/jsc_list.cpp
    jsc/jsc_map.cpp
)

target_include_directories(x6100js8
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../target
        ${CMAKE_CURRENT_SOURCE_DIR}/jsc
        ${CMAKE_CURRENT_SOURCE_DIR}/js8encode
)

target_link_libraries(x6100js8 PRIVATE m)
target_compile_options(x6100js8 PRIVATE -O2)
target_compile_definitions(x6100js8 PRIVATE JS8_NO_LIBSNDFILE)

set_target_properties(x6100js8 PROPERTIES
    VERSION   0.3.0
    SOVERSION 0
)

include(GNUInstallDirs)

install(TARGETS x6100js8
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/x6100js8
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/libx6100js8.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/libx6100js8.pc
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libx6100js8.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
