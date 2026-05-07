cmake_minimum_required(VERSION 3.18)
project(js8-hb-gen CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# fate's Makefile uses /opt/local (macOS Homebrew) — strip those paths.
# We find deps through pkg-config instead.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED sndfile)
pkg_check_modules(FFTW3 REQUIRED fftw3)

# --- encoder: TX-only, no decoder, no FFTW ---
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

# --- decoder: links fate's full RX stack (js8.cc + fft.cc + osd.cc) ---
add_executable(decode-test
    decode_test.cc
    fate/js8.cc
    fate/fft.cc
    fate/osd.cc
    fate/libldpc.cc
    fate/util.cc
    fate/unpack.cc
)

target_include_directories(decode-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SNDFILE_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
)

target_link_libraries(decode-test PRIVATE
    ${SNDFILE_LIBRARIES}
    ${FFTW3_LIBRARIES}
    m
    pthread
)

target_compile_options(decode-test PRIVATE -O2)

# Copy words.txt to the build dir so decode-test can load it at runtime.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/fate/words.txt
               ${CMAKE_CURRENT_BINARY_DIR}/words.txt COPYONLY)
