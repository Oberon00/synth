# Part of the synth tool -- Copyright (c) Christian Neumüller 2016
# This file is subject to the terms of the MIT License.
# See LICENSE.txt or http://opensource.org/licenses/MIT

set(libsynth_HDRS
    "CgStr.hpp"
    "DoxytagResolver.hpp"
    "FileIdSupport.hpp"
    "MultiTuProcessor.hpp"
    "SimpleTemplate.hpp"
    "annotate.hpp"
    "cgWrappers.hpp"
    "config.hpp"
    "debug.hpp"
    "highlight.hpp"
    "output.hpp"
    "xref.hpp"
)

set(libsynth_SRCS
    "DoxytagResolver.cpp"
    "MultiTuProcessor.cpp"
    "SimpleTemplate.cpp"
    "annotate.cpp"
    "debug.cpp"
    "highlight.cpp"
    "output.cpp"
    "xref.cpp"
)

set (synth_HDRS "cmdline.hpp")
set (synth_SRCS "cmdline.cpp" "main.cpp")

set (sycgdbg_HDRS)
set (sycgdbg_SRCS "dbgmain.cpp")

set(CMAKE_DEBUG_POSTFIX "-d")

add_library(synth ${libsynth_SRCS} ${libsynth_HDRS})

add_executable(synth-bin ${synth_SRCS} ${synth_HDRS})
set_target_properties(synth-bin PROPERTIES
    OUTPUT_NAME synth)
add_executable(sy-cgdbg ${sycgdbg_SRCS} ${sycgdbg_HDRS})
target_link_libraries(synth
    ${LLVM_LIBRARIES}
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(synth-bin synth)
target_link_libraries(sy-cgdbg synth)

install(TARGETS synth-bin RUNTIME DESTINATION bin)
