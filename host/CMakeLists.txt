cmake_minimum_required(VERSION 3.18)
project(js8-hb-gen CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# fate's Makefile uses /opt/local (macOS Homebrew) — strip those paths.
# We find deps through pkg-config instead.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED sndfile)

add_executable(js8-hb-gen
    main.cc
    recode.cc
    fate/pack.cc
    fate/libldpc.cc
    fate/util.cc
    fate/unpack.cc
)

target_include_directories(js8-hb-gen PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SNDFILE_INCLUDE_DIRS}
)

target_link_libraries(js8-hb-gen PRIVATE
    ${SNDFILE_LIBRARIES}
    m
)

target_compile_options(js8-hb-gen PRIVATE -O2)
