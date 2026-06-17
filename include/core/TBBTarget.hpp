#pragma once

#include "core/IR.hpp"
#include "clang/AST/ASTContext.h"
#include "clang/Frontend/CompilerInstance.h"

// Prints the IR program as Cilk1-style CPS code on top of the classic Intel
// TBB low-level task API (tbb::task, TBB <= 2020). Task functions become
// tbb::task subclasses; spawn_next closures become continuation tasks.
void PrintTBB(IRProgram &P, llvm::raw_ostream &out, clang::ASTContext &C,
              clang::CompilerInstance &CI);

// Emits a CMakeLists.txt that builds the generated file <AppName>.cpp into
// an executable named <AppName>, linking against a classic TBB build.
void PrintTBBCMake(const std::string &AppName, llvm::raw_ostream &Out);
