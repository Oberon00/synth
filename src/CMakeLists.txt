# Part of the synth tool -- Copyright (c) Christian Neumüller 2015
# This file is subject to the terms of the BSD 2-Clause License.
# See LICENSE.txt or http://opensource.org/licenses/BSD-2-Clause

set(synth_HDRS
    "CgStr.hpp"
    "FileIdSet.hpp"
    "HighlightedFile.hpp"
    "MultiTuProcessor.hpp"
    "SymbolDeclaration.hpp"
    "cmdline.hpp"
)

set(synth_SRCS
    "HighlightedFile.cpp"
    "MultiTuProcessor.cpp"
    "annotate.cpp"
    "cmdline.cpp"
    "main.cpp"
    "xref.cpp"
)

set(CMAKE_DEBUG_POSTFIX "-d")

add_executable(synth ${synth_SRCS} ${synth_HDRS})
target_link_libraries(synth ${LLVM_LIBRARIES} ${Boost_LIBRARIES})
#if(THREADS_HAVE_PTHREAD_ARG)
#  target_compile_options(PUBLIC synth "-pthread")
#endif()
#if(CMAKE_THREAD_LIBS_INIT)
#  target_link_libraries(synth "${CMAKE_THREAD_LIBS_INIT}")
#endif()
install(TARGETS synth RUNTIME DESTINATION bin)
