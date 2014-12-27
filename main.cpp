/*
 * Monicelli: an esoteric language compiler
 *
 * Copyright (C) 2014 Stefano Sanfilippo
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Scanner.hpp"
#include "Parser.hpp"

#include <iostream>
#include <fstream>
#include <stdint.h>
#include <vector>
#include <string>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Host.h>
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/ADT/SmallVector.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticIDs.h>
#include <clang/Basic/TargetOptions.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/DriverDiagnostic.h>

#include <clang/Frontend/TextDiagnosticPrinter.h>

#define OUTPUT_PATH "Intermediary.cpp"
#define OUT_FILE "a.out"

using namespace monicelli;

int main(int argc, char **argv) {
    Program program;
    Scanner scanner(std::cin);
    Parser parser(scanner, program);
    std::ofstream intermediaryFile(OUTPUT_PATH);

#if YYDEBUG
    parser.set_debug_level(1);
#endif

    parser.parse();
    program.emit(dynamic_cast<std::ostream &>(intermediaryFile));
    intermediaryFile.close();

    // Try to compile directly to an executable.
    std::string outputPath(OUTPUT_PATH);
    std::string clangPath = llvm::sys::FindProgramByName("clang");

    std::vector<const char *> clangArgs;
    clangArgs.push_back(clangPath.c_str());
    clangArgs.push_back(OUTPUT_PATH);
    clangArgs.push_back("-std=c++11");
    clangArgs.push_back("-x");
    clangArgs.push_back("c++");
    clangArgs.push_back("-v");

    llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> refPtr( new clang::DiagnosticIDs() );
    clang::DiagnosticOptions options = clang::DiagnosticOptions();
    clang::DiagnosticsEngine engine(refPtr, & options);
    clang::driver::Driver driver(clangArgs[0], llvm::sys::getDefaultTargetTriple(), OUT_FILE, engine);

    // Create a compiler
    clang::driver::Compilation * compilation = driver.BuildCompilation(clangArgs);
    llvm::SmallVector<std::pair<int, const clang::driver::Command *>, 0> failures;
    driver.ExecuteCompilation(*compilation, failures);

    return 0;
}

