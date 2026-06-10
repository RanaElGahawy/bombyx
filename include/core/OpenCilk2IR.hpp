#pragma once
#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTFwd.h>
#include <clang/AST/Decl.h>
#include <clang/Basic/SourceManager.h>
#include <unordered_map>

#include "core/IR.hpp"

extern std::set<std::string> GIgnoreFns;
extern std::vector<clang::RecordDecl *> GRecordDecls;

// Maps a Task/TaskCaller FunctionDecl to its IGNORE'd caller
// Populated by OpenCilk2IR and consumed by HardCilkTarget to generate the
// driver
using DriverCallersTy = std::unordered_map<const clang::FunctionDecl *,
                                           const clang::FunctionDecl *>;

void OpenCilk2IR(IRProgram &P, clang::ASTContext *Context, SourceManager &SM,
                 DriverCallersTy &DriverCallers);