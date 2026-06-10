#include "VitisHLSTarget.hpp"
#include "IR.hpp"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "llvm/Support/raw_ostream.h"

/////////////////////////////////////////////////////////////////////////
//                        Size Analysis Functions                      //
//   Recover the size expression for a pointer variable so the driver  //
//   can emit the right allocateMemFPGA and copyToDevice calls.        //
/////////////////////////////////////////////////////////////////////////

// 1:  extract size from VarDecl initialized as T *arr = new T[n] we return n.
static std::string getSizeExpr(const clang::ValueDecl *VD,
                               const std::string &BaseName,
                               const clang::SourceManager &SM,
                               const clang::LangOptions &LO) {

  // TODO: Check first if the Recod type has a size() method
  //  if (VD->getType()->isRecordType())
  //    return BaseName + ".size()";

  if (auto *Var = clang::dyn_cast<clang::VarDecl>(VD)) {
    if (auto *Init = Var->getInit()) {
      auto *NE =
          clang::dyn_cast<clang::CXXNewExpr>(Init->IgnoreParenImpCasts());
      if (NE && NE->isArray()) {
        if (auto SzOpt = NE->getArraySize()) {
          auto Text = clang::Lexer::getSourceText(
              clang::CharSourceRange::getTokenRange((*SzOpt)->getSourceRange()),
              SM, LO);
          if (!Text.empty())
            return Text.str();
        }
      }
    }
  }
  return "";
}

// 2: scan body until cutoff for cases when initliazion is separate from
// definition. For example:
// T *arr;
// ...
// arr = new T[n];
// return n
static std::string findAssignedSize(const std::string &VarName,
                                    const clang::CompoundStmt *Body,
                                    const clang::CallExpr *Cutoff,
                                    const clang::SourceManager &SM,
                                    const clang::LangOptions &LO) {
  struct Finder : clang::RecursiveASTVisitor<Finder> {
    const std::string &Name;
    std::string Found;
    const clang::SourceManager &SM;
    const clang::LangOptions &LO;
    Finder(const std::string &N, const clang::SourceManager &SM,
           const clang::LangOptions &LO)
        : Name(N), SM(SM), LO(LO) {}
    bool VisitBinaryOperator(clang::BinaryOperator *BO) {
      if (BO->getOpcode() != clang::BO_Assign)
        return true;
      auto *DRE = clang::dyn_cast<clang::DeclRefExpr>(
          BO->getLHS()->IgnoreParenImpCasts());
      if (!DRE || !DRE->getDecl()->getDeclName().isIdentifier() ||
          DRE->getDecl()->getName() != Name)
        return true;
      auto *NE = clang::dyn_cast<clang::CXXNewExpr>(
          BO->getRHS()->IgnoreParenImpCasts());
      if (!NE || !NE->isArray())
        return true;
      if (auto SzOpt = NE->getArraySize()) {
        auto Text = clang::Lexer::getSourceText(
            clang::CharSourceRange::getTokenRange((*SzOpt)->getSourceRange()),
            SM, LO);
        if (!Text.empty()) {
          Found = Text.str();
          return false;
        }
      }
      return true;
    }
  };
  for (auto *S : Body->body()) {
    if (SM.isPointWithin(Cutoff->getBeginLoc(), S->getBeginLoc(),
                         S->getEndLoc()))
      break;
    Finder f(VarName, SM, LO);
    f.TraverseStmt(S);
    if (!f.Found.empty())
      return f.Found;
  }
  return "";
}

// 3: scan body for any `new T[n]`; return n if unique.
// TODO: need to make sure that pointer is assigned to the pointer we are
// interested in not just a random pointer in the main
// static std::string findSizeByPointeeType(const std::string &PointeeTypeName,
//                                          const clang::CompoundStmt *Body,
//                                          const clang::CallExpr *Cutoff,
//                                          const clang::SourceManager &SM,
//                                          const clang::LangOptions &LO) {
//   struct Finder : clang::RecursiveASTVisitor<Finder> {
//     const std::string &Type;
//     std::set<std::string> Sizes;
//     const clang::SourceManager &SM;
//     const clang::LangOptions &LO;
//     Finder(const std::string &T, const clang::SourceManager &SM,
//            const clang::LangOptions &LO)
//         : Type(T), SM(SM), LO(LO) {}
//     bool VisitCXXNewExpr(clang::CXXNewExpr *NE) {
//       if (!NE->isArray() || NE->getAllocatedType().getAsString() != Type)
//         return true;
//       if (auto SzOpt = NE->getArraySize()) {
//         auto Text = clang::Lexer::getSourceText(
//             clang::CharSourceRange::getTokenRange((*SzOpt)->getSourceRange()),
//             SM, LO);
//         if (!Text.empty())
//           Sizes.insert(Text.str());
//       }
//       return true;
//     }
//   } f(PointeeTypeName, SM, LO);
//   for (auto *S : Body->body()) {
//     if (SM.isPointWithin(Cutoff->getBeginLoc(), S->getBeginLoc(),
//                          S->getEndLoc()))
//       break;
//     f.TraverseStmt(S);
//   }
//   return f.Sizes.size() == 1 ? *f.Sizes.begin() : "";
// }

// 4: look inside called functions for `arr = new T[n]` where arr
// is a VarName passed by reference.
static std::string findSizeFromCallee(const std::string &VarName,
                                      const clang::CompoundStmt *Body,
                                      const clang::CallExpr *Cutoff,
                                      const clang::SourceManager &SM,
                                      const clang::LangOptions &LO) {
  struct Searcher : clang::RecursiveASTVisitor<Searcher> {
    const std::string &VarName;
    std::string Result;
    const clang::SourceManager &SM;
    const clang::LangOptions &LO;
    Searcher(const std::string &V, const clang::SourceManager &SM,
             const clang::LangOptions &LO)
        : VarName(V), SM(SM), LO(LO) {}
    bool VisitCallExpr(clang::CallExpr *CE) {
      auto *FD =
          clang::dyn_cast_or_null<clang::FunctionDecl>(CE->getCalleeDecl());
      if (!FD || !FD->hasBody())
        return true;
      for (unsigned i = 0; i < CE->getNumArgs() && i < FD->getNumParams();
           ++i) {
        auto *DRE = clang::dyn_cast<clang::DeclRefExpr>(
            CE->getArg(i)->IgnoreParenImpCasts());
        if (!DRE || !DRE->getDecl()->getDeclName().isIdentifier())
          continue;
        if (DRE->getDecl()->getName() != VarName)
          continue;
        auto *Param = FD->getParamDecl(i);
        if (!Param->getType()->isReferenceType() ||
            !Param->getDeclName().isIdentifier())
          continue;
        std::string PName = Param->getName().str();
        struct InnerFinder : clang::RecursiveASTVisitor<InnerFinder> {
          const std::string &PName;
          std::string Found;
          const clang::SourceManager &SM;
          const clang::LangOptions &LO;
          InnerFinder(const std::string &N, const clang::SourceManager &SM,
                      const clang::LangOptions &LO)
              : PName(N), SM(SM), LO(LO) {}
          bool VisitBinaryOperator(clang::BinaryOperator *BO) {
            if (BO->getOpcode() != clang::BO_Assign)
              return true;
            auto *D = clang::dyn_cast<clang::DeclRefExpr>(
                BO->getLHS()->IgnoreParenImpCasts());
            if (!D || !D->getDecl()->getDeclName().isIdentifier() ||
                D->getDecl()->getName() != PName)
              return true;
            auto *NE = clang::dyn_cast<clang::CXXNewExpr>(
                BO->getRHS()->IgnoreParenImpCasts());
            if (!NE || !NE->isArray())
              return true;
            if (auto SzOpt = NE->getArraySize()) {
              auto Text = clang::Lexer::getSourceText(
                  clang::CharSourceRange::getTokenRange(
                      (*SzOpt)->getSourceRange()),
                  SM, LO);
              if (!Text.empty()) {
                Found = Text.str();
                return false;
              }
            }
            return true;
          }
        } inner(PName, SM, LO);
        inner.TraverseStmt(FD->getBody());
        if (!inner.Found.empty()) {
          Result = inner.Found;
          return false;
        }
      }
      return true;
    }
  };
  for (auto *S : Body->body()) {
    if (SM.isPointWithin(Cutoff->getBeginLoc(), S->getBeginLoc(),
                         S->getEndLoc()))
      break;
    Searcher s(VarName, SM, LO);
    s.TraverseStmt(S);
    if (!s.Result.empty())
      return s.Result;
  }
  return "";
}

///////////////////////
// Printer Helpers   //
///////////////////////

void VitisHLSTarget::PrintStartSystem(llvm::raw_ostream &Out) {
  Out << "        startSystem();\n";
}

void VitisHLSTarget::PrintManagementLoop(llvm::raw_ostream &Out) {
  Out << "        auto start_management = "
         "std::chrono::high_resolution_clock::now();\n";
  Out << "        managementLoop();\n";
  Out << "        auto end_management = "
         "std::chrono::high_resolution_clock::now();\n";
  Out << "        std::chrono::duration<double> management_duration = "
         "end_management - start_management;\n";
  Out << "        std::cout << \"Time taken by management_loop: \" << "
         "management_duration.count() << \" seconds\" << std::endl;\n";
}

void VitisHLSTarget::PrintAllocateMemFPGA(llvm::raw_ostream &Out,
                                          const std::string &AddrVar,
                                          const std::string &SizeExpr,
                                          const std::string &Alignment) {
  Out << "        uint64_t " << AddrVar << " = allocateMemFPGA(" << SizeExpr
      << ", " << Alignment << ");\n";
}

void VitisHLSTarget::PrintCopyToDevice(llvm::raw_ostream &Out,
                                       const std::string &Addr,
                                       const std::string &Data,
                                       const std::string &SizeExpr) {
  Out << "        memory_->copyToDevice(" << Addr << ", "
      << "reinterpret_cast<const uint8_t *>(" << Data << "), " << SizeExpr
      << ");\n";
}

void VitisHLSTarget::PrintCopyFromDevice(llvm::raw_ostream &Out,
                                         const std::string &Data,
                                         const std::string &Addr,
                                         const std::string &SizeExpr) {
  Out << "        memory_->copyFromDevice("
      << "reinterpret_cast<uint8_t *>(" << Data << "), " << Addr << ", "
      << SizeExpr << ");\n";
}

//////////////////////////
// Build Driver Header  //
//////////////////////////

DriverSpec VitisHLSTarget::BuildDriverSpec(clang::ASTContext &C) {
  DriverSpec Spec;

  // 1. Find the non-synthetic root task.
  IRFunction *RootFn = nullptr;
  for (auto &[T, Info] : TaskInfos) {
    if (Info.IsRoot && !Info.IsSynthetic) {
      RootFn = T;
      break;
    }
  }
  if (!RootFn) {
    llvm::errs() << "warning: could not find root task for driver header\n";
    return Spec;
  }

  auto &SM = C.getSourceManager();
  auto &LO = C.getLangOpts();

  const clang::FunctionDecl *OriginalFD = RootFn->Info.RootFun;

  // 2  Helper: find the first CallExpr that calls Target.
  // Used later for the cutoff and to get the correct arg names
  struct FindCallInFD : clang::RecursiveASTVisitor<FindCallInFD> {
    const clang::FunctionDecl *Target;
    const clang::CallExpr *Found = nullptr;
    FindCallInFD(const clang::FunctionDecl *T)
        : Target(T->getCanonicalDecl()) {}
    bool VisitCallExpr(clang::CallExpr *CE) {
      auto *FD =
          clang::dyn_cast_or_null<clang::FunctionDecl>(CE->getCalleeDecl());
      if (FD && FD->getCanonicalDecl() == Target) {
        Found = CE;
        return false;
      }
      return true;
    }
  };
  auto findCallIn =
      [&](const clang::FunctionDecl *InFD,
          const clang::FunctionDecl *Target) -> const clang::CallExpr * {
    if (!InFD || !InFD->hasBody() || !Target)
      return nullptr;
    FindCallInFD finder(Target);
    finder.TraverseStmt(InFD->getBody());
    return finder.Found;
  };

  // 3  ArgCallerFD / ArgRootCall: find the function that directly spawns
  // RootFn.
  //
  //  Two cases:
  //  A) Wrapper (randomWalk):  main → oneWalkPerNode_wrapper → [root]
  //     The spawner is a different function from OriginalFD, visible in the IR.
  //     SpawnList search finds it: F->Info.RootFun ≠ OriginalFD.
  //
  //  B) Direct entry (pageRank): main → pageRank → [FlattenIR synthetics]
  //     pageRank_ir is in P and its SpawnList contains RootFn, but
  //     pageRank_ir->Info.RootFun == OriginalFD (same function).
  //     Matching it would make findCallIn search for pageRank calling itself
  //     → null.  We skip it and fall through to DriverCallers instead.
  const clang::FunctionDecl *ArgCallerFD = nullptr;
  const clang::CallExpr *ArgRootCall = nullptr;
  if (OriginalFD) {
    for (auto &F : P) {
      if (!F->Info.SpawnList.count(RootFn) || !F->Info.RootFun)
        continue;
      // Skip FlattenIR versions of OriginalFD spawning their own continuations.
      if (F->Info.RootFun->getCanonicalDecl() == OriginalFD->getCanonicalDecl())
        continue;
      ArgCallerFD = F->Info.RootFun;
      ArgRootCall = findCallIn(ArgCallerFD, OriginalFD);
      break;
    }
    if (!ArgCallerFD) {
      // Case B: OriginalFD is the entry point so look it up in DriverCallers.
      auto It = DriverCallers.find(OriginalFD->getCanonicalDecl());
      if (It != DriverCallers.end()) {
        ArgCallerFD = It->second;
        ArgRootCall = findCallIn(ArgCallerFD, OriginalFD);
      }
    }
  }

  // 4  BodyCallerFD/BodyCutoff: climb thin wrappers if no set up was found.
  const clang::FunctionDecl *BodyCallerFD = ArgCallerFD;
  const clang::CallExpr *BodyCutoff = ArgRootCall;
  if (ArgCallerFD && ArgRootCall) {
    auto hasPreCallSetup = [&](const clang::FunctionDecl *FD,
                               const clang::CallExpr *Cutoff) -> bool {
      if (!FD->hasBody())
        return false;
      auto *Body = clang::cast<clang::CompoundStmt>(FD->getBody());
      for (auto *S : Body->body()) {
        if (SM.isPointWithin(Cutoff->getBeginLoc(), S->getBeginLoc(),
                             S->getEndLoc()))
          break;
        return true;
      }
      return false;
    };
    while (!hasPreCallSetup(BodyCallerFD, BodyCutoff)) {
      auto It = DriverCallers.find(BodyCallerFD->getCanonicalDecl());
      if (It == DriverCallers.end())
        break;
      const clang::FunctionDecl *NewCaller = It->second;
      const clang::CallExpr *NewCutoff = findCallIn(NewCaller, BodyCallerFD);
      if (!NewCutoff)
        break;
      BodyCallerFD = NewCaller;
      BodyCutoff = NewCutoff;
    }
  }

  // 5  Wrapper param→arg mapto resolve name conflicts between the wrapper and
  // the wrapper caller.
  std::map<const clang::ParmVarDecl *, const clang::Expr *> ParamToBodyArg;
  if (ArgCallerFD && BodyCutoff && ArgCallerFD != BodyCallerFD) {
    for (unsigned i = 0; i < BodyCutoff->getNumArgs() &&
                         i < (unsigned)ArgCallerFD->getNumParams();
         ++i)
      ParamToBodyArg[ArgCallerFD->getParamDecl(i)] =
          BodyCutoff->getArg(i)->IgnoreParenImpCasts();
  }
  auto resolveArgDRE =
      [&](const clang::DeclRefExpr *DRE) -> const clang::DeclRefExpr * {
    if (!DRE)
      return nullptr;
    if (auto *PVD = clang::dyn_cast<clang::ParmVarDecl>(DRE->getDecl())) {
      auto It = ParamToBodyArg.find(PVD);
      if (It != ParamToBodyArg.end())
        if (auto *R = clang::dyn_cast<clang::DeclRefExpr>(It->second))
          return R;
    }
    return DRE;
  };

  // extract source text for a statement (including trailing ';').
  auto stmtText = [&](const clang::Stmt *S) -> std::string {
    clang::SourceLocation End =
        clang::Lexer::getLocForEndOfToken(S->getEndLoc(), 0, SM, LO);
    if (const char *NC = SM.getCharacterData(End); NC && *NC == ';')
      End = End.getLocWithOffset(1);
    return clang::Lexer::getSourceText(
               clang::CharSourceRange::getCharRange(S->getBeginLoc(), End), SM,
               LO)
        .str();
  };

  //////////////////////////
  // Populate DriverSpec  //
  //////////////////////////

  Spec.ClassName =
      (OriginalFD ? OriginalFD->getNameAsString() : RootFn->getName()) +
      "Driver";
  Spec.TaskStructName = RootFn->getName() + "_task";

  // 6  function calls used in the root caller (e.g. main) needs to be printed
  // before the driver class.
  if (BodyCallerFD && BodyCallerFD->hasBody()) {
    struct FuncCollector : clang::RecursiveASTVisitor<FuncCollector> {
      std::vector<const clang::FunctionDecl *> &Decls;
      std::set<const clang::FunctionDecl *> Seen;
      const clang::SourceManager &SM;
      const IRProgram &P;
      FuncCollector(std::vector<const clang::FunctionDecl *> &D,
                    const clang::SourceManager &SM, const IRProgram &P)
          : Decls(D), SM(SM), P(P) {}
      bool VisitCallExpr(clang::CallExpr *CE) {
        auto *FD =
            clang::dyn_cast_or_null<clang::FunctionDecl>(CE->getCalleeDecl());
        if (!FD || SM.isInSystemHeader(FD->getBeginLoc()))
          return true;
        const clang::FunctionDecl *Def = FD->getDefinition();
        if (!Def || !Def->hasBody())
          return true;
        for (auto &F : P)
          if (F->Info.RootFun &&
              F->Info.RootFun->getCanonicalDecl() == Def->getCanonicalDecl())
            return true;
        if (Seen.insert(Def).second)
          Decls.push_back(Def);
        return true;
      }
    };
    std::vector<const clang::FunctionDecl *> HelperFDs;
    FuncCollector fc(HelperFDs, SM, P);
    fc.TraverseStmt(BodyCallerFD->getBody());
    for (auto *FD : HelperFDs) {
      if (!FD->hasBody())
        continue;
      clang::SourceLocation End = clang::Lexer::getLocForEndOfToken(
          FD->getBody()->getEndLoc(), 0, SM, LO);
      auto Text = clang::Lexer::getSourceText(
          clang::CharSourceRange::getCharRange(FD->getBeginLoc(), End), SM, LO);
      if (!Text.empty())
        Spec.HelperFunctions.push_back(Text.str());
    }
  }

  // 7  if globals are used add their declations.
  if (BodyCallerFD && BodyCallerFD->hasBody()) {
    struct GlobalCollector : clang::RecursiveASTVisitor<GlobalCollector> {
      std::vector<std::string> &Result;
      std::set<const clang::VarDecl *> Seen;
      const clang::SourceManager &SM;
      const clang::LangOptions &LO;
      GlobalCollector(std::vector<std::string> &R,
                      const clang::SourceManager &SM,
                      const clang::LangOptions &LO)
          : Result(R), SM(SM), LO(LO) {}
      bool VisitDeclRefExpr(clang::DeclRefExpr *DRE) {
        auto *VD = clang::dyn_cast<clang::VarDecl>(DRE->getDecl());
        if (VD &&
            clang::isa<clang::TranslationUnitDecl>(VD->getDeclContext()) &&
            Seen.insert(VD).second) {
          auto Text = clang::Lexer::getSourceText(
              clang::CharSourceRange::getTokenRange(VD->getBeginLoc(),
                                                    VD->getLocation()),
              SM, LO);
          if (!Text.empty())
            Result.push_back(Text.str());
        }
        return true;
      }
    } gc(Spec.ExternGlobals, SM, LO);
    gc.TraverseStmt(BodyCallerFD->getBody());
  }

  // 8  count zero fields for the continuation.
  std::vector<const IRVarDecl *> ArgVars;
  for (auto &Var : RootFn->Vars)
    if (Var.DeclLoc == IRVarDecl::ARG)
      ArgVars.push_back(&Var);
  Spec.NumZeroFields = 1 + ArgVars.size(); // _cont + each arg
  Spec.HasPadding = TaskInfos.at(RootFn).TaskPadding > 0;
  if (Spec.HasPadding)
    Spec.NumZeroFields++;

  // 9  Pointer args (BaseName, PointeeType, SizeExpr).
  std::set<const clang::ValueDecl *> AllocatedBases;
  if (ArgRootCall && BodyCallerFD && BodyCallerFD->hasBody()) {
    auto *CallerBody =
        clang::cast<clang::CompoundStmt>(BodyCallerFD->getBody());
    for (size_t i = 0; i < ArgVars.size() && i < ArgRootCall->getNumArgs();
         ++i) {
      if (!ArgVars[i]->Type->isPointerType())
        continue;
      const clang::Expr *Arg = ArgRootCall->getArg(i)->IgnoreParenImpCasts();
      auto *RawDRE = clang::dyn_cast<clang::DeclRefExpr>(Arg);
      if (!RawDRE)
        continue;
      auto *DRE = resolveArgDRE(RawDRE);
      if (!AllocatedBases.insert(DRE->getDecl()).second)
        continue;

      std::string BaseName = DRE->getDecl()->getName().str();
      std::string PointeeType =
          ArgVars[i]->Type->getPointeeType().getAsString();

      std::string SizeExpr = getSizeExpr(DRE->getDecl(), BaseName, SM, LO);
      if (SizeExpr.empty())
        SizeExpr = findAssignedSize(BaseName, CallerBody, BodyCutoff, SM, LO);
      // TODO: FIX THIS in th eprevious todo
      // if (SizeExpr.empty())
      //   SizeExpr =
      //       findSizeByPointeeType(PointeeType, CallerBody, BodyCutoff, SM,
      //       LO);
      if (SizeExpr.empty())
        SizeExpr = findSizeFromCallee(BaseName, CallerBody, BodyCutoff, SM, LO);

      // TODO: find another fall back
      // if (SizeExpr.empty())
      //   SizeExpr = BaseName + "_count";

      Spec.PtrArgs.push_back(
          {BaseName, PointeeType, SizeExpr, /*CopyBack=*/false});
    }
  }

  // 10  Mark pointers that are referenced after the spawn (need
  // copyFromDevice).
  if (BodyCutoff && BodyCallerFD && BodyCallerFD->hasBody()) {
    std::set<std::string> PostCallNames;
    auto *Body = clang::cast<clang::CompoundStmt>(BodyCallerFD->getBody());
    bool PastCall = false;
    struct RefCollector : clang::RecursiveASTVisitor<RefCollector> {
      std::set<std::string> &Names;
      RefCollector(std::set<std::string> &N) : Names(N) {}
      bool VisitDeclRefExpr(clang::DeclRefExpr *DRE) {
        if (DRE->getDecl()->getDeclName().isIdentifier())
          Names.insert(DRE->getDecl()->getName().str());
        return true;
      }
    };
    for (auto *S : Body->body()) {
      if (!PastCall) {
        if (SM.isPointWithin(BodyCutoff->getBeginLoc(), S->getBeginLoc(),
                             S->getEndLoc()))
          PastCall = true;
        continue;
      }
      RefCollector rc(PostCallNames);
      rc.TraverseStmt(S);
    }
    for (auto &PA : Spec.PtrArgs)
      PA.CopyBack = PostCallNames.count(PA.BaseName) > 0;
  }

  // 11  Pre-call statement texts.
  if (BodyCutoff && BodyCallerFD && BodyCallerFD->hasBody()) {
    auto *Body = clang::cast<clang::CompoundStmt>(BodyCallerFD->getBody());
    for (auto *S : Body->body()) {
      if (SM.isPointWithin(BodyCutoff->getBeginLoc(), S->getBeginLoc(),
                           S->getEndLoc()))
        break;
      Spec.PreCallStmts.push_back(stmtText(S));
    }
  }

  // 12  Post-call statement texts (skip return).
  if (BodyCutoff && BodyCallerFD && BodyCallerFD->hasBody()) {
    auto *Body = clang::cast<clang::CompoundStmt>(BodyCallerFD->getBody());
    bool PastCall = false;
    for (auto *S : Body->body()) {
      if (!PastCall) {
        if (SM.isPointWithin(BodyCutoff->getBeginLoc(), S->getBeginLoc(),
                             S->getEndLoc()))
          PastCall = true;
        continue;
      }
      if (clang::isa<clang::ReturnStmt>(S))
        continue;
      Spec.PostCallStmts.push_back(stmtText(S));
    }
  }

  // 13  Task struct field assignments.
  Spec.FieldAssignments.push_back({"_cont", "addr"});
  if (ArgRootCall) {
    for (size_t i = 0; i < ArgVars.size() && i < ArgRootCall->getNumArgs();
         ++i) {
      std::string Field = GetSym(ArgVars[i]->Name);
      const clang::Expr *Arg = ArgRootCall->getArg(i)->IgnoreParenImpCasts();
      std::string Value;

      if (ArgVars[i]->Type->isPointerType()) {
        if (auto *RawDRE = clang::dyn_cast<clang::DeclRefExpr>(Arg)) {
          auto *DRE = resolveArgDRE(RawDRE);
          Value = DRE->getDecl()->getName().str() + "_addr";
        } else if (auto *BO = clang::dyn_cast<clang::BinaryOperator>(Arg)) {
          clang::Expr *LHS = BO->getLHS()->IgnoreParenImpCasts();
          clang::Expr *RHS = BO->getRHS()->IgnoreParenImpCasts();
          std::string PT = ArgVars[i]->Type->getPointeeType().getAsString();
          char Op = BO->getOpcode() == clang::BO_Add ? '+' : '-';
          if (auto *BaseDRE = clang::dyn_cast<clang::DeclRefExpr>(LHS)) {
            auto Off = clang::Lexer::getSourceText(
                clang::CharSourceRange::getTokenRange(RHS->getSourceRange()),
                SM, LO);
            Value = resolveArgDRE(BaseDRE)->getDecl()->getName().str() +
                    "_addr " + Op + " sizeof(" + PT + ") * (" + Off.str() + ")";
          } else if (auto *BaseDRE = clang::dyn_cast<clang::DeclRefExpr>(RHS)) {
            auto Off = clang::Lexer::getSourceText(
                clang::CharSourceRange::getTokenRange(LHS->getSourceRange()),
                SM, LO);
            Value = resolveArgDRE(BaseDRE)->getDecl()->getName().str() +
                    "_addr + sizeof(" + PT + ") * (" + Off.str() + ")";
          }
        }
      } else {
        const clang::Expr *Resolved = Arg;
        if (auto *DRE = clang::dyn_cast<clang::DeclRefExpr>(Arg))
          if (auto *R = resolveArgDRE(DRE); R != DRE)
            Resolved = R;
        Value =
            clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(
                                            Resolved->getSourceRange()),
                                        SM, LO)
                .str();
      }
      if (!Value.empty())
        Spec.FieldAssignments.push_back({Field, Value});
    }
  }
  if (Spec.HasPadding)
    Spec.FieldAssignments.push_back({"_padding", "0"});

  return Spec;
}

void VitisHLSTarget::PrintDriverHeader(llvm::raw_ostream &Out,
                                       clang::ASTContext &C) {
  DriverSpec Spec = BuildDriverSpec(C);

  // File headers.
  Out << "#pragma once\n\n";
  Out << "#include <hardCilkDriver.h>\n";
  Out << "#include \"" << AppName << "_defs.h\"\n\n";

  // Helper function definitions.
  for (auto &Fn : Spec.HelperFunctions)
    Out << Fn << "\n\n";

  // Class + method header.
  Out << "class " << Spec.ClassName << " : public hardCilkDriver {\n";
  Out << "public:\n";
  // TODO: Do we need argv and argc?
  Out << "    " << Spec.ClassName
      << "(Memory *memory, int argc, char *argv[]) : hardCilkDriver(memory) "
         "{}\n\n";
  Out << "    int run_test_bench() override {\n";

  // TODO: change with exiting continuation if there is one
  //  Zero-initialised task struct + root-task allocation.
  Out << "        " << Spec.TaskStructName << " root_task_0 = {";
  for (size_t i = 0; i < Spec.NumZeroFields; i++) {
    if (i > 0)
      Out << ", ";
    Out << "0";
  }
  Out << "};\n";
  Out << "        int counter = 2;\n\n";
  PrintAllocateMemFPGA(Out, "addr", "sizeof(root_task_0)",
                       "sizeof(root_task_0)");
  PrintCopyToDevice(Out, "addr", "&root_task_0", "sizeof(root_task_0)");
  PrintCopyToDevice(Out, "addr", "&counter", "sizeof(counter)");
  Out << "\n";

  // Extern declarations for globals.
  // TODO: do we need extern?
  if (!Spec.ExternGlobals.empty()) {
    for (auto &G : Spec.ExternGlobals)
      Out << "        " << G << ";\n";
    Out << "\n";
  }

  // Statments before the function call.
  for (auto &Stmt : Spec.PreCallStmts)
    Out << "        " << Stmt << "\n";

  // Pointer allocations + copyToDevice.
  Out << "\n";
  for (auto &PA : Spec.PtrArgs) {
    std::string SzStr = "sizeof(" + PA.PointeeType + ") * " + PA.SizeExpr;
    PrintAllocateMemFPGA(Out, PA.BaseName + "_addr", SzStr, "512");
    PrintCopyToDevice(Out, PA.BaseName + "_addr", PA.BaseName, SzStr);
  }

  // Task struct field assignments.
  Out << "\n";
  for (auto &[Field, Value] : Spec.FieldAssignments)
    Out << "        root_task_0." << Field << " = " << Value << ";\n";

  // Start the system and measure the management loop.
  Out << "\n";
  PrintStartSystem(Out);
  Out << "\n";
  PrintManagementLoop(Out);

  // opyFromDevice for pointers referenced after the spawn.
  Out << "\n";
  for (auto &PA : Spec.PtrArgs) {
    if (!PA.CopyBack)
      continue;
    PrintCopyFromDevice(Out, PA.BaseName, PA.BaseName + "_addr",
                        "sizeof(" + PA.PointeeType + ") * " + PA.SizeExpr);
  }

  // Statments after the function call.
  Out << "\n";
  for (auto &Stmt : Spec.PostCallStmts)
    Out << "        " << Stmt << "\n";

  Out << "\n        return 0;\n";
  Out << "    }\n";
  Out << "};\n";
}
