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
#include "ProgramOptions.hpp"

#include <iostream>
#include <fstream>
#include <stdint.h>
#include <vector>
#include <string>

#ifdef LLVM_ENABLED
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Host.h>
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/LinkAllPasses.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticIDs.h>
#include <clang/Basic/TargetOptions.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/DriverDiagnostic.h>

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>

#include <clang/Driver/DriverDiagnostic.h>
#include <clang/Driver/Options.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/FrontendDiagnostic.h>
#include <clang/Frontend/TextDiagnosticBuffer.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Frontend/Utils.h>
#include <clang/FrontendTool/Utils.h>
#include <llvm/ADT/Statistic.h>
#include <llvm/LinkAllPasses.h>
#include <llvm/Option/ArgList.h>
#include <llvm/Option/OptTable.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Timer.h>
#include <llvm/Support/raw_ostream.h>


#define OUTPUT_PATH "Intermediate.cpp"
#define OUT_FILE "a.out"


static void LLVMErrorHandler(void *data, const std::string & message, bool generateCrash) {
    fprintf(stderr, "FATAL: %s\n", message.c_str());
    llvm::sys::RunInterruptHandlers();

    // We cannot recover from llvm errors.
    exit(-1);
}

#endif

using namespace monicelli;

int main(int argc, char **argv) {
    Program program;
    ProgramOptions programOptions(argc, argv);

    std::ifstream input("/dev/stdin");
    std::ofstream output("/dev/stdout");

    // Chain everything and parse.
    programOptions.
        addOption("--input", "-i", "Input file", "input").
        addOption("--output", "-o", "Output file", "output").
        parse();

    if (programOptions.optionParsed("input")) {
        std::string inputString = programOptions.getValueAsString("input");
        if (inputString != "-")
            input = std::ifstream(inputString);
    }

    if (programOptions.optionParsed("output")) {
        std::string outputString = programOptions.getValueAsString("output");
        if (outputString != "-")
            output = std::ofstream(outputString);
    }

    Scanner scanner(dynamic_cast<std::istream &>(input));
    Parser parser(scanner, program);

#if YYDEBUG
    parser.set_debug_level(1);
#endif

    parser.parse();

#ifdef LLVM_ENABLED
    std::ofstream intermediaryFile(OUTPUT_PATH);
    program.emit(dynamic_cast<std::ostream &>(intermediaryFile));
    intermediaryFile.close();

    std::unique_ptr<clang::CompilerInstance> compiler(new clang::CompilerInstance());
    clang::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagnosticIDs(new clang::DiagnosticIDs());

    // Init llvm
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();

    // Arguments for Clang
    clang::SmallVector<const char *, 256> clangArgs;

    std::string outputPath(OUTPUT_PATH);

    clangArgs.push_back("-std=c++11");
    clangArgs.push_back("-x");
    clangArgs.push_back("c++");
    clangArgs.push_back("-cc1");

    // Input file
    clangArgs.push_back(OUTPUT_PATH);

    // Create a compilation object
    clang::IntrusiveRefCntPtr<clang::DiagnosticOptions> diagnosticOptions = new clang::DiagnosticOptions();
    clang::TextDiagnosticBuffer * diagnosticBuffer = new clang::TextDiagnosticBuffer();
    clang::DiagnosticsEngine diagnosticsEngine(diagnosticIDs, &*diagnosticOptions, diagnosticBuffer);
    bool success = clang::CompilerInvocation::CreateFromArgs(
        compiler->getInvocation(), clangArgs.begin(), clangArgs.end(), diagnosticsEngine);

    compiler->createDiagnostics();
    if (not compiler->hasDiagnostics()) {
        fprintf(stderr, "Could not create diagnostics.\n");
        return -1;
    }

    llvm::install_fatal_error_handler(LLVMErrorHandler, static_cast<void*>(&compiler->getDiagnostics()));
    diagnosticBuffer->FlushDiagnostics(compiler->getDiagnostics());
    if (not success) {
        fprintf(stderr, "Could not create a compiler's invocation\n");
        return -1;
    }

    // Execute the compilation.
    success = clang::ExecuteCompilerInvocation(compiler.get());

    // Stop all the timers.
    llvm::TimerGroup::printAll(llvm::errs());
    llvm::remove_fatal_error_handler();

    // Shutdown LLVM
    llvm::llvm_shutdown();

    if (not success) {
        fprintf(stderr, "Compilation failed\n");
    }
#else
    program.emit(dynamic_cast<std::ostream &>(output));
#endif
    return 0;
}

