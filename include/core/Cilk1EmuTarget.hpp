#pragma once

#include "core/IR.hpp"
#include "clang/AST/ASTContext.h"
#include "clang/Frontend/CompilerInstance.h"

void PrintCilk1Emu(IRProgram &P, llvm::raw_ostream &out, clang::ASTContext &C,
                   clang::CompilerInstance &CI);

// Shared helpers (defined in Cilk1EmuTarget.cpp) also used by other
// CPS software backends (e.g. the TBB target).
void printLocals(IRFunction *F, clang::ASTContext &C, llvm::raw_ostream &Out);
void printOriginalSourceSplit(IRProgram &P, llvm::raw_ostream &OutA,
                              llvm::raw_ostream &OutB, clang::ASTContext &C,
                              clang::CompilerInstance &CI);