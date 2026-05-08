# Top-level project — buildroot's cmake-package builder enters here.
#
# Only the library is buildroot-relevant. host/ and target/ are dev
# scratch trees containing the host PoC and the cross-compiled smoke
# test binary respectively, neither of which gets shipped on the radio.

cmake_minimum_required(VERSION 3.18)
project(x6100-js8-engine CXX)

add_subdirectory(lib)
