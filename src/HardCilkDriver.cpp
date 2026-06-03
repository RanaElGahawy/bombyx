#include "HardCilkTarget.hpp"
#include "IR.hpp"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "llvm/Support/raw_ostream.h"

// Derive the count/size expression for a pointer variable:
//   - new T[n]   → extract "n" from the CXXNewExpr
//   - container  → "basename.size()"
//   - fallback   → "basename_count"  (user fills in)
static std::string getSizeExpr(const clang::ValueDecl *VD,
                               const std::string &BaseName,
                               const clang::SourceManager &SM,
                               const clang::LangOptions &LO) {
  if (VD->getType()->isRecordType())
    return BaseName + ".size()";

  if (auto *Var = clang::dyn_cast<clang::VarDecl>(VD)) {
    if (auto *Init = Var->getInit()) {
      auto *NewExpr =
          clang::dyn_cast<clang::CXXNewExpr>(Init->IgnoreParenImpCasts());
      if (NewExpr && NewExpr->isArray()) {
        if (auto SizeOpt = NewExpr->getArraySize()) {
          auto Text =
              clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(
                                              (*SizeOpt)->getSourceRange()),
                                          SM, LO);
          if (!Text.empty())
            return Text.str();
        }
      }
    }
  }
  return BaseName + "_count";
}

void HardCilkTarget::PrintDriverHeader(llvm::raw_ostream &Out,
                                       clang::ASTContext &C) {
  // Find the root task function (spawned from a non-task caller).
  IRFunction *RootFn = nullptr;
  for (auto &[T, Info] : TaskInfos) {
    if (Info.IsRoot && !Info.IsSynthetic) {
      RootFn = T;
      break;
    }
  }
  if (!RootFn) {
    llvm::errs() << "warning: could not find root task for driver header\n";
    return;
  }

  // Find the non-task caller that spawns the root task (the "main" equivalent).
  IRFunction *CallerFn = nullptr;
  for (auto &F : P) {
    if (F->Info.IsTask)
      continue;
    for (auto *G : F->Info.SpawnList) {
      if (G == RootFn) {
        CallerFn = F.get();
        break;
      }
    }
    if (CallerFn)
      break;
  }

  std::string RootName = RootFn->getName();
  std::string ClassName = RootName + "Driver";
  std::string TaskStructName = RootName + "_task";

  Out << "#pragma once\n\n";
  Out << "#include <hardCilkDriver.h>\n";
  Out << "#include \"" << AppName << "_defs.h\"\n\n";

  Out << "class " << ClassName << " : public hardCilkDriver {\n";
  Out << "public:\n";
  Out << "    " << ClassName
      << "(Memory *memory, char *argv[]) : hardCilkDriver(memory) {}\n\n";
  Out << "    int run_test_bench() override {\n";

  // Count fields: _cont + each arg + padding array (if any)
  size_t NumZeros = 1;
  for (auto &Var : RootFn->Vars)
    if (Var.DeclLoc == IRVarDecl::ARG)
      NumZeros++;
  if (TaskInfos[RootFn].TaskPadding > 0)
    NumZeros++;

  Out << "        " << TaskStructName << " root_task_0 = {";
  for (size_t i = 0; i < NumZeros; i++) {
    if (i > 0)
      Out << ", ";
    Out << "0";
  }
  Out << "};\n";

  Out << "        int counter = 2;\n\n";
  Out << "        uint64_t addr = allocateMemFPGA(sizeof(root_task_0), "
         "sizeof(root_task_0));\n";
  Out << "        memory_->copyToDevice(addr, reinterpret_cast<const uint8_t "
         "*>(&root_task_0), sizeof(root_task_0));\n";
  Out << "        memory_->copyToDevice(addr, reinterpret_cast<const uint8_t "
         "*>(&counter), sizeof(counter));\n\n";

  auto &SM = C.getSourceManager();
  auto &LO = C.getLangOpts();

  // Find the CallExpr for the root task so we can inspect actual arguments
  // and use its location as the copy cutoff.
  const clang::CallExpr *RootCall = nullptr;
  if (CallerFn && CallerFn->Info.RootFun && RootFn->Info.RootFun) {
    struct FindCall : clang::RecursiveASTVisitor<FindCall> {
      const clang::FunctionDecl *Target;
      const clang::CallExpr *Found = nullptr;
      FindCall(const clang::FunctionDecl *T) : Target(T->getCanonicalDecl()) {}
      bool VisitCallExpr(clang::CallExpr *CE) {
        auto *FD =
            clang::dyn_cast_or_null<clang::FunctionDecl>(CE->getCalleeDecl());
        if (FD && FD->getCanonicalDecl() == Target) {
          Found = CE;
          return false;
        }
        return true;
      }
    } finder(RootFn->Info.RootFun);
    finder.TraverseStmt(CallerFn->Info.RootFun->getBody());
    RootCall = finder.Found;
  }

  // Copy statements from the original main body up to the
  // statement that contains the root task call.
  // Skip if RootCall is null.
  if (RootCall && CallerFn && CallerFn->Info.RootFun &&
      CallerFn->Info.RootFun->hasBody()) {
    auto *Body =
        clang::cast<clang::CompoundStmt>(CallerFn->Info.RootFun->getBody());
    for (auto *S : Body->body()) {
      if (RootCall && SM.isPointWithin(RootCall->getBeginLoc(),
                                       S->getBeginLoc(), S->getEndLoc()))
        break;

      clang::SourceLocation End =
          clang::Lexer::getLocForEndOfToken(S->getEndLoc(), 0, SM, LO);
      const char *NextChar = SM.getCharacterData(End);
      if (NextChar && *NextChar == ';')
        End = End.getLocWithOffset(1);
      auto Text = clang::Lexer::getSourceText(
          clang::CharSourceRange::getCharRange(S->getBeginLoc(), End), SM, LO);
      Out << "        " << Text << "\n";
    }
  }

  // Collect arg vars in declaration order
  std::vector<const IRVarDecl *> ArgVars;
  for (auto &Var : RootFn->Vars)
    if (Var.DeclLoc == IRVarDecl::ARG)
      ArgVars.push_back(&Var);

  // --- allocate + copy for base pointer arguments ---
  Out << "\n";
  std::set<const clang::ValueDecl *> AllocatedBases;
  // (baseName, pointeeType, sizeExpr)
  std::vector<std::tuple<std::string, std::string, std::string>> AllocatedPtrs;
  if (RootCall) {
    for (size_t i = 0; i < ArgVars.size() && i < RootCall->getNumArgs(); ++i) {
      if (!ArgVars[i]->Type->isPointerType())
        continue;
      const clang::Expr *Arg = RootCall->getArg(i)->IgnoreParenImpCasts();
      auto *DRE = clang::dyn_cast<clang::DeclRefExpr>(Arg);
      if (!DRE || !AllocatedBases.insert(DRE->getDecl()).second)
        continue;
      std::string BaseName = DRE->getDecl()->getName().str();
      std::string PointeeType =
          ArgVars[i]->Type->getPointeeType().getAsString();
      std::string SizeExpr = getSizeExpr(DRE->getDecl(), BaseName, SM, LO);
      AllocatedPtrs.push_back({BaseName, PointeeType, SizeExpr});
      Out << "        uint64_t " << BaseName << "_addr = allocateMemFPGA("
          << "sizeof(" << PointeeType << ") * " << SizeExpr << ", 512);\n";
      Out << "        memory_->copyToDevice(" << BaseName << "_addr, "
          << "reinterpret_cast<const uint8_t *>(" << BaseName << "), "
          << "sizeof(" << PointeeType << ") * " << SizeExpr << ");\n";
    }
  }

  // --- reassign root_task_0 fields with real addresses ---
  Out << "\n";
  Out << "        root_task_0._cont = addr;\n";
  if (RootCall) {
    for (size_t i = 0; i < ArgVars.size() && i < RootCall->getNumArgs(); ++i) {
      std::string FieldName = GetSym(ArgVars[i]->Name);
      const clang::Expr *Arg = RootCall->getArg(i)->IgnoreParenImpCasts();

      if (ArgVars[i]->Type->isPointerType()) {
        if (auto *DRE = clang::dyn_cast<clang::DeclRefExpr>(Arg)) {
          // pointer: use its allocated address.
          Out << "        root_task_0." << FieldName << " = "
              << DRE->getDecl()->getName().str() << "_addr;\n";
        } else if (auto *BO = clang::dyn_cast<clang::BinaryOperator>(Arg)) {
          // Derived pointer (base ± offset): translate to base_addr ±
          // sizeof(T)*offset.
          clang::Expr *LHS = BO->getLHS()->IgnoreParenImpCasts();
          clang::Expr *RHS = BO->getRHS()->IgnoreParenImpCasts();
          std::string PointeeType =
              ArgVars[i]->Type->getPointeeType().getAsString();
          char Op = BO->getOpcode() == clang::BO_Add ? '+' : '-';

          // Pointer is on the LHS (common: base + offset).
          if (auto *BaseDRE = clang::dyn_cast<clang::DeclRefExpr>(LHS)) {
            auto OffText = clang::Lexer::getSourceText(
                clang::CharSourceRange::getTokenRange(RHS->getSourceRange()),
                SM, LO);
            Out << "        root_task_0." << FieldName << " = "
                << BaseDRE->getDecl()->getName().str() << "_addr " << Op
                << " sizeof(" << PointeeType << ") * (" << OffText << ");\n";
          } else if (auto *BaseDRE = clang::dyn_cast<clang::DeclRefExpr>(RHS)) {
            // offset + base (less common).
            auto OffText = clang::Lexer::getSourceText(
                clang::CharSourceRange::getTokenRange(LHS->getSourceRange()),
                SM, LO);
            Out << "        root_task_0." << FieldName << " = "
                << BaseDRE->getDecl()->getName().str() << "_addr + sizeof("
                << PointeeType << ") * (" << OffText << ");\n";
          }
        }
      } else {
        // normal arg -> copy the original expression text directly.
        auto Text = clang::Lexer::getSourceText(
            clang::CharSourceRange::getTokenRange(Arg->getSourceRange()), SM,
            LO);
        Out << "        root_task_0." << FieldName << " = " << Text << ";\n";
      }
    }
  }

  if (TaskInfos[RootFn].TaskPadding > 0)
    Out << "        root_task_0._padding = 0;\n";

  Out << "\n";
  Out << "        // Start the system\n";
  Out << "        startSystem();\n\n";
  Out << "        // Measure time for the management loop\n";
  Out << "        auto start_management = "
         "std::chrono::high_resolution_clock::now();\n";
  Out << "        managementLoop();\n";
  Out << "        auto end_management = "
         "std::chrono::high_resolution_clock::now();\n";
  Out << "        std::chrono::duration<double> management_duration = "
         "end_management - start_management;\n";
  Out << "        std::cout << \"Time taken by management_loop: \" << "
         "management_duration.count() << \" seconds\" << std::endl;\n";

  // Read pointer data back from the FPGA after execution.
  Out << "\n";
  for (auto &[BaseName, PointeeType, SizeExpr] : AllocatedPtrs) {
    Out << "        memory_->copyFromDevice("
        << "reinterpret_cast<uint8_t *>(" << BaseName << "), " << BaseName
        << "_addr, "
        << "sizeof(" << PointeeType << ") * " << SizeExpr << ");\n";
  }

  // Copy statements from the caller body that come after the root task call.
  if (RootCall && CallerFn && CallerFn->Info.RootFun &&
      CallerFn->Info.RootFun->hasBody()) {
    auto *Body =
        clang::cast<clang::CompoundStmt>(CallerFn->Info.RootFun->getBody());
    bool PastCall = false;
    Out << "\n";
    for (auto *S : Body->body()) {
      if (!PastCall) {
        if (SM.isPointWithin(RootCall->getBeginLoc(), S->getBeginLoc(),
                             S->getEndLoc()))
          PastCall = true;
        continue;
      }
      if (clang::isa<clang::ReturnStmt>(S))
        continue;
      clang::SourceLocation End =
          clang::Lexer::getLocForEndOfToken(S->getEndLoc(), 0, SM, LO);
      const char *NextChar = SM.getCharacterData(End);
      if (NextChar && *NextChar == ';')
        End = End.getLocWithOffset(1);
      auto Text = clang::Lexer::getSourceText(
          clang::CharSourceRange::getCharRange(S->getBeginLoc(), End), SM, LO);
      Out << "        " << Text << "\n";
    }
  }

  Out << "\n        return 0;\n";
  Out << "    }\n";
  Out << "};\n";
}
