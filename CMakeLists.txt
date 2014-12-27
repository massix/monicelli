#
# Monicelli: an esoteric language compiler
# 
# Copyright (C) 2014 Stefano Sanfilippo
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

project(Monicelli)
cmake_minimum_required(VERSION 2.8)

message("== Only a limited set of platforms was tested. We need your help!")
message("== Report build errors to https://github.com/esseks/monicelli/issues")

## 1. Compiler sanity check

try_compile(
    supported_compiler
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/features.cpp
    COMPILE_DEFINITIONS -std=c++0x
    OUTPUT_VARIABLE features_build_log
)

if (checkfeat)
    message(${features_build_log})
endif()

if (NOT supported_compiler)
    message(FATAL_ERROR
        "Some C++11 features we need are not implemented by your compiler.\n"
        "Run cmake with -Dcheckfeat=1 to see the exact cause."
    )
endif()

## 2. Find Flex and Bison

find_package(BISON REQUIRED)
find_package(FLEX 2.5 REQUIRED)

if (BISON_VERSION VERSION_LESS 2.5)
    message(FATAL_ERROR "At least Bison 2.5 is required.")
elseif(BISON_VERSION VERSION_LESS 3.0)
    message("== Bison 2.5 was found. You have to apply cmake/bison2.patch...")
endif()

## 3. Optionally find llvm-config
find_program(LLVM_CONFIG llvm-config-3.4 llvm-config-3.5 llvm-config)

if (LLVM_CONFIG)
    execute_process(COMMAND ${LLVM_CONFIG} --version
                    OUTPUT_VARIABLE LLVM_VERSION ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(COMMAND ${LLVM_CONFIG} --cppflags
                    OUTPUT_VARIABLE LLVM_CXX_FLAGS ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(COMMAND ${LLVM_CONFIG} --ldflags
                    OUTPUT_VARIABLE LLVM_LINKER_FLAGS ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(COMMAND ${LLVM_CONFIG} --libs all
                    OUTPUT_VARIABLE LLVM_LIBRARIES ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

    message("== LLVM Config ${LLVM_VERSION} found at ${LLVM_CONFIG}")
    ## message("== Adding ${LLVM_CXX_FLAGS} to compiler's arguments")
    ## message("== Adding ${LLVM_LINKER_FLAGS} ${LLVM_LIBRARIES} to linker's arguments")
    add_definitions(-DLLVM_ENABLED)

    ## Fastest and dirtiest way to do it.
    ## TODO: Write a FindLLVM.cmake
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LLVM_LINKER_FLAGS} ${LLVM_LIBRARIES}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lclang -lclangAnalysis -lclangEdit -lclangAST -lclangDriver -lclangIndex -lclangParse -lclangLex -lclangBasic -lclangFrontend -lclangFrontendTool -lclangSerialization -lclangCodeGen -lclangSema -lclangRewriteFrontend -lclangRewriteCore -lclangFormat -lclangARCMigrate -lclangStaticAnalyzerCheckers -lclangStaticAnalyzerFrontend -lclangStaticAnalyzerCore")
else()
    message("== LLVM Config not found")
endif()

## 4. Build

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_definitions(-Wall -Wno-deprecated-register -std=c++0x -DYYDEBUG=0 -O2)

bison_target(Parser Monicelli.ypp ${CMAKE_CURRENT_BINARY_DIR}/Parser.cpp)
flex_target(Scanner Monicelli.lpp ${CMAKE_CURRENT_BINARY_DIR}/Lexer.cpp)
add_flex_bison_dependency(Scanner Parser)

add_executable(
    mcc
    main.cpp ProgramOptions.cpp Nodes.cpp ${BISON_Parser_OUTPUTS} ${FLEX_Scanner_OUTPUTS}
)

