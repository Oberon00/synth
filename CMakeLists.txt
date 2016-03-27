# Part of the synth tool -- Copyright (c) Christian Neumüller 2015
# This file is subject to the terms of the BSD 2-Clause License.
# See LICENSE.txt or http://opensource.org/licenses/BSD-2-Clause

cmake_minimum_required(VERSION 2.8)
project(synth)

# http://stackoverflow.com/a/13104057/2128694
macro(prepend_to_all _srcs _path)
    unset(_tmp)
    foreach(src ${${_srcs}})
        list(APPEND _tmp ${_path}${src})
    endforeach()
    set(${_srcs} ${_tmp})
endmacro()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (MSVC)
    add_definitions("/W4" "/MP")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    add_definitions("-D__STDC_CONSTANT_MACROS" "-D__STDC_LIMIT_MACROS")
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        #set (_cfgfile
        #    "${CMAKE_CURRENT_SOURCE_DIR}/include/apollo/detail/clang_stdlib_config.hpp")
        #add_definitions("-DBOOST_STDLIB_CONFIG=\"${_cfgfile}\"")
        add_definitions("-Weverything")
        add_definitions("-Wno-c++98-compat -Wno-c++98-compat-pedantic")
        add_definitions("-Wno-weak-vtables -Wno-padded")
        add_definitions("-Wno-global-constructors")

        ## LLVM Headers
        #add_definitions("-Wno-conversion")
        #add_definitions("-Wno-sign-conversion")
        #add_definitions("-Wno-shorten-64-to-32")
        #add_definitions("-Wno-old-style-cast")
        #add_definitions("-Wno-unused-parameter")
        #add_definitions("-Wno-documentation-unknown-command")
        #add_definitions("-Wno-shadow")
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_definitions("-Wall -Wextra -pedantic")
        add_definitions("-Winit-self -Woverloaded-virtual")
        add_definitions("-Wlogical-op")
        add_definitions("-Wmissing-declarations")
        add_definitions("-Wsign-conversion -Wsign-promo -Wconversion")
        add_definitions("-Wcast-align -Wcast-qual -Wold-style-cast")
        add_definitions("-Wstrict-overflow=5")
        add_definitions("-Wstrict-null-sentinel")
        add_definitions("-Wformat=2")
        add_definitions("-Wmissing-include-dirs")
        add_definitions("-Wdisabled-optimization")
        add_definitions("-Wredundant-decls")
        add_definitions("-Wshadow")
        add_definitions("-Wundef")
        add_definitions("-Wswitch-default")
        add_definitions("-Wnoexcept")

        add_definitions("-Wno-missing-field-initializers")
    endif()
endif()

set (LLVM_LIB_DIR "/usr/lib/llvm-3.8/lib/")

#find_path(LIBLLVM_INCLUDE_DIR
#    "Support/CommandLine.h"
#    PATHS "/usr/include/llvm-3.8/llvm/")
#find_path(LIBCLANG_INCLUDE_DIR
#    "clang/Tooling/Tooling.h"
#    PATHS "/usr/lib/llvm-3.8/include/")
#
#set (llvm_lib_names
#    clangAST
#    clangBasic
#    clangDriver
#    clangFrontend
#    clangSerialization
#    clangRewriteFrontend
#    clangStaticAnalyzerFrontend
#    clangTooling
#    LLVMOption
#    LLVMSupport)
#
#foreach(libname ${llvm_lib_names})
#    find_library(${libname}_libpath
#        PATHS ${LLVM_LIB_DIR}
#        NAMES ${libname})
#    message(STATUS "${libname} -> ${${libname}_libpath}")
#    list(APPEND LLVM_LIBRARIES ${${libname}_libpath})
#endforeach()


find_path(LIBCLANG_INCLUDE_DIR
    "clang-c/Index.h"
    PATHS "/usr/include/llvm-3.8/llvm/")

find_library(LLVM_LIBRARIES
    PATHS ${LLVM_LIB_DIR}
    NAMES clang)

find_package(Boost REQUIRED filesystem system)
include_directories(SYSTEM ${LIBCLANG_INCLUDE_DIR} ${Boost_INCLUDE_DIRS})
add_subdirectory("src")
