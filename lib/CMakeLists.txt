# libx6100js8 — JS8 protocol engine library for the X6100.
#
# Builds a shared library installed to /usr/lib/libx6100js8.so with the
# public header at /usr/include/x6100js8/x6100js8.h. Mirrors the layout
# ft8lib uses on this radio.

cmake_minimum_required(VERSION 3.18)
project(x6100js8 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Library sources fall into three groups:
#   * fate    — original encoder primitives, MIT (rtmrtmrtmrtm/fate)
#   * jsc     — STL port of JS8Call's word-dictionary compressor + 524K
#               lines of verbatim const dictionary tables, GPL-3
#   * src     — our public C API + frame builder for directed messages
#
# Mixing GPL-3 (jsc) with MIT (fate) is permitted in one direction only:
# the combined library is GPL-3-or-later. The repo LICENSE header at top
# level reflects this.
add_library(x6100js8 SHARED
    src/x6100js8.cc
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

# pkg-config so x6100_gui's CMake can find us via pkg_check_modules.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/libx6100js8.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/libx6100js8.pc
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libx6100js8.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
