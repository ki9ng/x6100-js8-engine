cmake_minimum_required(VERSION 3.18)
project(js8-hb-test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# fate's encoder side: pack.cc + libldpc.cc + util.cc + unpack.cc.
# pack.cc's pack_text() / pack_directed() reach into unpack.cc for shared
# varicode tables, so unpack.cc has to come along even on a TX-only build.
add_executable(js8-hb-test
    src/js8_hb_test.cc
    src/wav_writer.cc
    src/recode.cc
    fate/pack.cc
    fate/libldpc.cc
    fate/util.cc
    fate/unpack.cc
)

target_include_directories(js8-hb-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(js8-hb-test PRIVATE
    m
)

# Match fate's host build optimisation level. -O3 is fine for ARMv7-A —
# the A33 has VFPv4 and NEON, the toolchain emits hardfloat by default.
target_compile_options(js8-hb-test PRIVATE -O2)
target_compile_definitions(js8-hb-test PRIVATE JS8_NO_LIBSNDFILE)

# Some of fate's STL-heavy code wants exceptions enabled even though we
# don't throw — disabling globally would break std::vector.
