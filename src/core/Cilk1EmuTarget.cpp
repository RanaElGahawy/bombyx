#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Type.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <cstring>
#include <llvm/Support/raw_ostream.h>
#include <set>

#include "core/Cilk1EmuTarget.hpp"
#include "core/IR.hpp"
#include "core/util.hpp"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExprCilk.h"
#include "clang/AST/Stmt.h"

#define TAB "    "

////////////////////////////////////////
// 1. Forward declarations, closures //
//////////////////////////////////////

void printFunDecl(IRFunction *F, llvm::raw_ostream &out, clang::ASTContext &C) {
  if (F->Info.IsTask) {
    out << "THREAD(" << F->getName() << ")";
  } else if (F->Info.RootFun) {
    out << F->Info.RootFun->getReturnType().getAsString();
    out << " " << F->Info.RootFun->getName() << "(";
    bool first = true;
    for (auto &Var : F->Vars) {
      if (Var.DeclLoc != IRVarDecl::ARG)
        continue;
      if (!first)
        out << ", ";
      else
        first = false;
      Var.Type.print(out, C.getPrintingPolicy(), GetSym(Var.Name));
    }
    out << ")";
  } else {
    PANIC("Non-root fun should be a task!");
  }
}

void printClosureDecl(IRFunction *F, llvm::raw_ostream &out,
                      clang::ASTContext &C,
                      const std::string &DataStruct = "") {
  if (!DataStruct.empty()) {
    out << "CLOSURE_DEF_SHARED(" << F->getName() << ", " << DataStruct
        << ");\n";
    return;
  }
  out << "CLOSURE_DEF(" << F->getName() << ",\n";
  for (auto &Var : F->Vars) {
    if (Var.DeclLoc == IRVarDecl::ARG) {
      out << TAB;
      Var.Type.print(out, C.getPrintingPolicy(), GetSym(Var.Name));
      out << ";\n";
    }
  }
  out << ");\n";
}

/////////////////////////////////////////////////////
// 2. Rewrite source to remove modified functions //
///////////////////////////////////////////////////

void printOriginalSource(IRProgram &P, llvm::raw_ostream &out,
                         clang::ASTContext &C, clang::CompilerInstance &CI) {
  Rewriter R;
  SourceManager &SM = CI.getSourceManager();
  R.setSourceMgr(SM, CI.getLangOpts());
  for (auto &F : P) {
    if (F->Info.RootFun) {
      R.RemoveText(F->Info.RootFun->getSourceRange());
    }
  }
  R.getEditBuffer(SM.getMainFileID()).write(out);
}

////////////////////////////
// 3. Print IR Functions //
//////////////////////////

void printLocals(IRFunction *F, clang::ASTContext &C, llvm::raw_ostream &Out) {
  for (auto &Local : F->Vars) {
    if (Local.DeclLoc == IRVarDecl::LOCAL) {
      Out << TAB;
      clang::QualType Ty = Local.Type;
      std::string Name = GetSym(Local.Name);
      // can we used it? it removes const, volatile, and restrict
      // otherwise we need more complicated code
      Ty = Ty.getUnqualifiedType();
      if (Ty->isArrayType()) {
        auto *AT = llvm::dyn_cast<clang::ConstantArrayType>(
            Ty->getAsArrayTypeUnsafe());
        if (AT) {
          AT->getElementType().print(Out, C.getPrintingPolicy());
          Out << " " << Name << "[" << AT->getSize().getZExtValue() << "];\n";
        } else {
          Ty.print(Out, C.getPrintingPolicy(), Name);
          Out << ";\n";
        }
      } else {
        Ty.print(Out, C.getPrintingPolicy(), Name);
        // Emit constructor-initialization args if the original decl used them.
        if (Local.ASTDecl && Local.ASTDecl->hasInit()) {
          auto *Init = Local.ASTDecl->getInit()->IgnoreParenImpCasts();
          if (auto *CE = llvm::dyn_cast<clang::CXXConstructExpr>(Init)) {
            if (!CE->isElidable() &&
                !CE->getConstructor()->isCopyConstructor() &&
                CE->getNumArgs() > 0) {
              Out << "(";
              for (unsigned I = 0; I < CE->getNumArgs(); ++I) {
                if (I > 0)
                  Out << ", ";
                CE->getArg(I)->printPretty(Out, nullptr, C.getPrintingPolicy());
              }
              Out << ")";
            }
          }
        }
        Out << ";\n";
      }
    }
  }
}

/*
struct DeclInfo {
  int ReuseCnt;
  enum { Arg, Local } Type;
};

//
https://stackoverflow.com/questions/32685540/why-cant-i-compile-an-unordered-map-with-a-pair-as-key
struct pair_hash {
  template <class T1, class T2>
  std::size_t operator () (const std::pair<T1,T2> &p) const {
      auto h1 = std::hash<T1>{}(p.first);
      auto h2 = std::hash<T2>{}(p.second);

      // Mainly for demonstration purposes, i.e. works but is overly simple
      // In the real world, use sth. like boost.hash_combine
      return h1 ^ h2;
  }
};

typedef std::unordered_map<std::pair<IRFunction*, IRVarRef>, DeclInfo,
pair_hash> DeclMap; #define DM_ENT(F,VR) (std::make_pair(F,VR))

void DeclMapInit(DeclMap &DM, IRFunction *F) {
  std::unordered_map<std::string, int> ReuseCnts;
  for (auto Arg : F->Args) {
    DM[DM_ENT(F, Arg)] = DeclInfo{.Type = DeclInfo::Arg};
    ReuseCnts[Arg->getDeclName().getAsString()] = 0;
  }
  for (auto Arg : F->Materialized) {
    DM[DM_ENT(F, Arg)] = DeclInfo{.Type = DeclInfo::Arg};
    ReuseCnts[Arg->getDeclName().getAsString()] = 0;
  }
  for (auto Local : F->Locals) {
    std::string LocalName = Local->getNameAsString();
    if (ReuseCnts.find(LocalName) == ReuseCnts.end()) {
      ReuseCnts[LocalName] = 0;
    } else {
      ReuseCnts[LocalName] += 1;
    }
    DM[DM_ENT(F, Local)] =
        DeclInfo{.ReuseCnt = ReuseCnts[LocalName], .Type = DeclInfo::Local};
  }
}

bool DeclMapLookup(DeclMap &DM, IRFunction *F, IRVarRef VR, std::string
&replaced) { if (DM.find(DM_ENT(F,VR)) == DM.end()) { return false;
  }
  auto &DI = DM[DM_ENT(F,VR)];

  switch (DI.Type) {
    case DeclInfo::Arg: {

      replaced = "largs->" + VR->getName().str();
      return true;
    }
    case DeclInfo::Local: {
      if (DI.ReuseCnt > 0) {
        replaced = VR->getName().str() + std::to_string(DI.ReuseCnt);
        return true;
      } else {
        return false;
      }
    }
  }
  return false;
}

void DeclMapPrint(DeclMap &DM) {
  for (auto const& [P, DI] : DM)  {
    auto const& [F, VR] = P;
    F->printName(llvm::outs());
    llvm::outs() << "," << VR->getName() << " -> ";
    if (DI.Type == DeclInfo::Arg) {
      llvm::outs() << "Arg\n";
    } else {
      llvm::outs() << "Local (RC " << DI.ReuseCnt << ")\n";
    }
  }
}



class StmtNameRemapper : public clang::RecursiveASTVisitor<StmtNameRemapper> {
private:
  DeclMap &DM;
  Rewriter &R;
public:
  IRFunction *F;
  explicit StmtNameRemapper(DeclMap &DM, Rewriter &R): DM(DM), R(R) {}

  bool VisitDeclRefExpr(clang::DeclRefExpr *Dre) {
    if (DM.find(DM_ENT(F, Dre->getDecl())) != DM.end()) {
      std::string replaced;
      if (DeclMapLookup(DM, F, Dre->getDecl(), replaced)) {
        R.ReplaceText(Dre->getSourceRange(), replaced);
      }
    }
    return true;
  }
}; */

class Cilk1EmuPrinter : public ScopedIRTraverser {
private:
  llvm::raw_ostream &Out;
  IRPrintContext &C;
  int SpawnCtr = 0;
  int IndentLvl = 1;
  std::set<ClosureDeclIRStmt *> DeclaredClosures;
  const std::unordered_map<IRFunction *, std::string> &FnDataStruct;

public:
  bool LastReturnPrinted = false;
  bool UsesMemcpy = false;

private:
  llvm::raw_ostream &Indent() {
    for (int i = 0; i < IndentLvl; i++)
      Out << TAB;
    return Out;
  }

  void handleScope(ScopeEvent SE) override {
    switch (SE) {
    case ScopeEvent::Close: {
      assert(IndentLvl > 0);
      IndentLvl--;
      Indent() << "}\n";
      break;
    }
    case ScopeEvent::Open: {
      Out << " {\n";
      IndentLvl++;
      break;
    }
    case ScopeEvent::Else: {
      assert(IndentLvl > 0);
      IndentLvl--;
      Indent() << "} else {\n";
      IndentLvl++;
      break;
    }
    default: {
    }
    }
  }

  void handleSpawnNextDecl(ClosureDeclIRStmt *DS, IRFunction *F) {
    if (DeclaredClosures.count(DS))
      return;
    DeclaredClosures.insert(DS);
    const std::string &SpawnNextFnName = DS->Fn->getName();
    Indent() << SpawnNextFnName << "_closure "
             << "SN_" << SpawnNextFnName << "c";
    if (F->Info.IsTask) {
      Out << "(largs->k);\n";
    } else {
      Out << "(CONT_DUMMY);\n";
    }
    Indent();
    Out << "spawn_next<" << SpawnNextFnName << "_closure> "
        << "SN_" << SpawnNextFnName << "(SN_" << SpawnNextFnName << "c);\n";
  }

  void handleSpawnNext(SpawnNextIRStmt *S, IRFunction *F,
                       IRBasicBlock *SNBlock) {
    const std::string &SpawnNextFnName = S->Fn->getName();
    std::set<IRVarRef> UnconditionalEphemeralVars;
    for (auto &B : *F) {
      for (auto &Stmt : *B) {
        if (auto *ES = dyn_cast<ESpawnIRStmt>(Stmt.get())) {
          if (ES->SN == S && ES->Local && ES->Dest) {
            if (auto *ID = dyn_cast<IdentIRExpr>(ES->Dest.get()))
              if (B.get() == SNBlock)
                UnconditionalEphemeralVars.insert(ID->Ident);
          }
        }
      }
    }
    // Check if all non-ephemeral fields are same-named ARG pass-throughs and
    // both F and the SN function share a data struct — if so, use bulk copy.
    auto SrcFamIt = FnDataStruct.find(F);
    auto DstFamIt = FnDataStruct.find(S->Fn);
    bool canBulk = F->Info.IsTask && SrcFamIt != FnDataStruct.end() &&
                   DstFamIt != FnDataStruct.end() &&
                   SrcFamIt->second == DstFamIt->second;
    if (canBulk) {
      for (auto &[SrcVar, DstVar] : S->Decl->Caller2Callee) {
        if (SrcVar->IsEphemeral && UnconditionalEphemeralVars.count(SrcVar))
          continue;
        if (SrcVar->DeclLoc != IRVarDecl::ARG ||
            GetSym(SrcVar->Name) != GetSym(DstVar->Name)) {
          canBulk = false;
          break;
        }
      }
    }
    if (canBulk) {
      Indent() << "*static_cast<" << SrcFamIt->second << "*>(SN_"
               << SpawnNextFnName << ".cls.get()) = *largs;\n";
    } else {
      for (auto &[SrcVar, DstVar] : S->Decl->Caller2Callee) {
        if (SrcVar->IsEphemeral && UnconditionalEphemeralVars.count(SrcVar))
          continue;
        if (SrcVar->Type->isArrayType()) {
          UsesMemcpy = true;
          Indent() << "std::memcpy(((" << SpawnNextFnName << "_closure*)SN_"
                   << SpawnNextFnName << ".cls.get())->" << GetSym(DstVar->Name)
                   << ", ";
          C.IdentCB(Out, SrcVar);
          Out << ", sizeof(";
          C.IdentCB(Out, SrcVar);
          Out << "));\n";
        } else {
          Indent() << "((" << SpawnNextFnName << "_closure*)SN_"
                   << SpawnNextFnName;
          Out << ".cls.get())->" << GetSym(DstVar->Name) << " = ";
          C.IdentCB(Out, SrcVar);
          Out << ";\n";
        }
      }
    }
    Indent() << "// Original sync was here\n";
  }

  // Returns the shared data struct name if F and ES->Fn are in the same family
  // AND every spawn arg is a direct same-named ARG pass-through from largs.
  std::string bulkCopyDataStruct(ESpawnIRStmt *ES, IRFunction *F) {
    if (!F->Info.IsTask)
      return "";
    auto SrcIt = FnDataStruct.find(F);
    auto DstIt = FnDataStruct.find(ES->Fn);
    if (SrcIt == FnDataStruct.end() || DstIt == FnDataStruct.end())
      return "";
    if (SrcIt->second != DstIt->second)
      return "";
    auto DstArgIt = ES->Fn->Vars.begin();
    for (auto &Arg : ES->Args) {
      while (DstArgIt != ES->Fn->Vars.end() &&
             DstArgIt->DeclLoc != IRVarDecl::ARG)
        DstArgIt++;
      if (DstArgIt == ES->Fn->Vars.end())
        return "";
      auto *IE = llvm::dyn_cast<IdentIRExpr>(Arg.get());
      if (!IE)
        return "";
      IRVarDecl *SrcVar = IE->Ident;
      if (!SrcVar || SrcVar->DeclLoc != IRVarDecl::ARG)
        return "";
      if (GetSym(SrcVar->Name) != GetSym(DstArgIt->Name))
        return "";
      DstArgIt++;
    }
    return SrcIt->second;
  }

  void emitSpawnArgList(ESpawnIRStmt *ES, const std::string &accessor) {
    auto DstArgIt = ES->Fn->Vars.begin();
    for (auto &Arg : ES->Args) {
      while (DstArgIt != ES->Fn->Vars.end() &&
             DstArgIt->DeclLoc != IRVarDecl::ARG)
        DstArgIt++;
      assert(DstArgIt != ES->Fn->Vars.end());
      auto &DstArg = *DstArgIt;
      if (DstArg.Type->isArrayType()) {
        UsesMemcpy = true;
        Indent() << "std::memcpy(sp" << SpawnCtr << "c" << accessor
                 << GetSym(DstArg.Name) << ", ";
        Arg->print(Out, C);
        Out << ", sizeof(sp" << SpawnCtr << "c" << accessor
            << GetSym(DstArg.Name) << "));\n";
      } else {
        Indent() << "sp" << SpawnCtr << "c" << accessor << GetSym(DstArg.Name)
                 << " = ";
        Arg->print(Out, C);
        Out << ";\n";
      }
      DstArgIt++;
    }
  }

  void handleSpawn(ESpawnIRStmt *ES, IRFunction *F) {
    const std::string &SpawnFnName = ES->Fn->getName();
    if (ES->SN) {
      Indent() << "cont sp" << SpawnCtr << "k;\n";
      if (ES->SN->Decl)
        handleSpawnNextDecl(ES->SN->Decl, F);
      const std::string &SpawnNextFnName = ES->SN->Fn->getName();
      if (ES->Dest) {
        if (ES->Local) {
          auto *IdentDest = dyn_cast<IdentIRExpr>(ES->Dest.get());
          assert(IdentDest);
          const std::string DestName = GetSym(IdentDest->Ident->Name);
          bool destIsArg = false;
          for (auto &V : ES->SN->Fn->Vars) {
            if (V.DeclLoc == IRVarDecl::ARG && GetSym(V.Name) == DestName) {
              destIsArg = true;
              break;
            }
          }
          if (destIsArg) {
            Indent() << "SN_BIND(SN_" << SpawnNextFnName << ", &sp" << SpawnCtr
                     << "k, " << DestName << ");\n";
          } else {
            Indent() << "SN_BIND_VOID(SN_" << SpawnNextFnName << ", &sp"
                     << SpawnCtr << "k);\n";
          }
        } else {
          Indent() << "SN_BIND_EXT(SN_" << SpawnNextFnName << ", &sp"
                   << SpawnCtr << "k, &(";
          ES->Dest->print(Out, C);
          Out << "));\n";
        }
      } else {
        Indent() << "SN_BIND_VOID(SN_" << SpawnNextFnName << ", &sp" << SpawnCtr
                 << "k);\n";
      }
    }

    if (ES->SN) {
      Indent() << SpawnFnName << "_closure sp" << SpawnCtr << "c(sp" << SpawnCtr
               << "k);\n";
      emitSpawnArgList(ES, ".");
    } else {
      // Fire-and-forget spawn (no spawn_next). Pass the parent's k
      // continuation so the spawned task can eventually SEND_ARGUMENT
      // back to the original caller.
      std::string DataStruct = bulkCopyDataStruct(ES, F);
      if (!DataStruct.empty()) {
        Indent() << "auto sp" << SpawnCtr << "c = std::make_shared<"
                 << SpawnFnName << "_closure>(largs->k, *largs);\n";
      } else {
        Indent() << "auto sp" << SpawnCtr << "c = std::make_shared<"
                 << SpawnFnName << "_closure>(";
        if (F->Info.IsTask) {
          Out << "largs->k";
        } else {
          Out << "CONT_DUMMY";
        }
        Out << ");\n";
        emitSpawnArgList(ES, "->");
      }
    }

    // we do not create spawn destination functions.
    // we expect them to be in argument first order
    // assert(ES->Fn->Info.RootFun);

    if (ES->SN) {
      Indent() << "spawn<" << SpawnFnName << "_closure> sp" << SpawnCtr << "(sp"
               << SpawnCtr << "c);\n\n";
    } else {
      Indent() << "cilk_spawn taskSpawn(sp" << SpawnCtr << "c->getTask(), sp"
               << SpawnCtr << "c);\n";
    }
  }

  void visitStmt(IRStmt *S, IRBasicBlock *B) {
    auto *F = B->getParent();
    if (S->Silent)
      return;

    if (auto *ES = dyn_cast<ESpawnIRStmt>(S)) {
      handleSpawn(ES, F);
      SpawnCtr++;
    } else if (auto *SNS = dyn_cast<SpawnNextIRStmt>(S)) {
      handleSpawnNext(SNS, F, B);
    } else if (auto *CDS = dyn_cast<ClosureDeclIRStmt>(S)) {
      handleSpawnNextDecl(CDS, F);
    } else if (auto *RS = dyn_cast<ReturnIRStmt>(S)) {
      Indent();
      LastReturnPrinted = true;
      if (F->Info.IsTask) {
        if (RS->RetVal) {
          Out << "SEND_ARGUMENT(largs->k, ";
          RS->RetVal->print(Out, C);
          Out << ");\n";
        } else {
          // Check if a fire-and-forget spawn already passed largs->k to a
          // spawned task; if so, that task owns signaling and we just return.
          bool inheritedByContinuation = false;
          for (auto &PrevS : *B) {
            if (PrevS.get() == S)
              break;
            if (auto *PES = dyn_cast<ESpawnIRStmt>(PrevS.get()))
              if (!PES->SN)
                inheritedByContinuation = true;
          }
          if (inheritedByContinuation)
            Out << "return;\n";
          else
            Out << "SEND_ARGUMENT(largs->k, 0);\n";
        }
      } else {
        RS->print(Out, C);
        Out << ";\n";
      }
    } else {
      Indent();
      S->print(Out, C);
      if (!isa<IfIRStmt>(S) && !isa<LoopIRStmt>(S)) {
        Out << ";\n";
      }
    }
  }

  void visitBlock(IRBasicBlock *B) override {

    for (auto &S : *B) {
      visitStmt(S.get(), B);
    }
    if (B->Term)
      visitStmt(B->Term, B);
  }

public:
  Cilk1EmuPrinter(llvm::raw_ostream &Out, IRPrintContext &C,
                  const std::unordered_map<IRFunction *, std::string> &FDS)
      : Out(Out), C(C), FnDataStruct(FDS) {}
};

void printOriginalSourceSplit(IRProgram &P, llvm::raw_ostream &OutA,
                              llvm::raw_ostream &OutB, clang::ASTContext &C,
                              clang::CompilerInstance &CI) {
  SourceManager &SM = CI.getSourceManager();
  Rewriter R;
  R.setSourceMgr(SM, CI.getLangOpts());

  // Remove task functions
  for (auto &F : P) {
    if (F->Info.RootFun)
      R.RemoveText(F->Info.RootFun->getSourceRange());
  }

  // Find the start location of the first function definition in the main file
  SourceLocation FirstFnLoc = SourceLocation();
  for (auto *D : C.getTranslationUnitDecl()->decls()) {
    if (!SM.isInMainFile(D->getLocation()))
      continue;
    if (auto *FD = llvm::dyn_cast<clang::FunctionDecl>(D)) {
      if (FD->getBody()) {
        SourceLocation Loc = FD->getBeginLoc();
        if (FirstFnLoc.isInvalid() ||
            SM.isBeforeInTranslationUnit(Loc, FirstFnLoc))
          FirstFnLoc = Loc;
      }
    }
  }

  // Get the full rewritten buffer
  const RewriteBuffer &Buf = R.getEditBuffer(SM.getMainFileID());
  std::string FullSource;
  llvm::raw_string_ostream SS(FullSource);
  Buf.write(SS);
  SS.flush();

  if (FirstFnLoc.isInvalid()) {
    // No functions — dump everything to A
    OutA << FullSource;
    return;
  }

  // Convert SourceLocation to offset in the original buffer
  unsigned SplitOffset = SM.getFileOffset(FirstFnLoc);

  // Part A: everything before the first function
  OutA << FullSource.substr(0, SplitOffset);

  // Part B: everything from the first function onward
  OutB << FullSource.substr(SplitOffset);
}

void PrintCilk1Emu(IRProgram &P, llvm::raw_ostream &out, clang::ASTContext &C,
                   clang::CompilerInstance &CI) {

  std::string Buf;
  llvm::raw_string_ostream BufStream(Buf);

  // Compute closure data families: task functions that share the same ordered
  // ARG list (name + type) can share a data struct, enabling one-liner copies.
  // Key: ordered vector of (field_name, type_string) pairs.
  using DataKey = std::vector<std::pair<std::string, std::string>>;
  std::map<DataKey, std::vector<IRFunction *>> FamilyMap;
  for (auto &F : P) {
    if (!F->Info.IsTask)
      continue;
    DataKey Key;
    for (auto &Var : F->Vars)
      if (Var.DeclLoc == IRVarDecl::ARG)
        Key.push_back(
            {GetSym(Var.Name), Var.Type.getAsString(C.getPrintingPolicy())});
    if (!Key.empty())
      FamilyMap[Key].push_back(F.get());
  }
  // Only keep groups with 2+ members — singletons need no shared struct.
  std::unordered_map<IRFunction *, std::string> FnDataStruct;
  for (auto &[Key, Fns] : FamilyMap) {
    if (Fns.size() < 2)
      continue;
    // Name the struct after the first member's RootFun (or the function
    // itself).
    IRFunction *First = Fns[0];
    // Name the struct after the first member's IR name (unique in the program).
    std::string DataName = First->getName() + "_data";
    for (auto *Fn : Fns)
      FnDataStruct[Fn] = DataName;
  }

  // 1. Print forward declarations of each function, include Cilk1 emulation
  // file.
  std::string PartB;
  llvm::raw_string_ostream PartBStream(PartB);
  printOriginalSourceSplit(P, BufStream, PartBStream, C, CI);

  for (auto &F : P) {
    printFunDecl(F.get(), BufStream, C);
    BufStream << ";\n";
  }
  BufStream << "\n";

  // Emit shared data structs before closure definitions.
  std::set<std::string> EmittedDataStructs;
  for (auto &F : P) {
    if (!F->Info.IsTask)
      continue;
    auto It = FnDataStruct.find(F.get());
    if (It == FnDataStruct.end())
      continue;
    const std::string &DataName = It->second;
    if (!EmittedDataStructs.insert(DataName).second)
      continue;
    BufStream << "struct " << DataName << " {\n";
    for (auto &Var : F->Vars) {
      if (Var.DeclLoc != IRVarDecl::ARG)
        continue;
      BufStream << TAB;
      Var.Type.print(BufStream, C.getPrintingPolicy(), GetSym(Var.Name));
      BufStream << ";\n";
    }
    BufStream << "};\n";
  }
  BufStream << "\n";

  for (auto &F : P) {
    if (F->Info.IsTask) {
      auto It = FnDataStruct.find(F.get());
      printClosureDecl(F.get(), BufStream, C,
                       It != FnDataStruct.end() ? It->second : "");
    }
  }

  // 2. Print the original source file with the original root functions
  // removed.
  BufStream << PartBStream.str();
  BufStream << "\n";

  bool UsesMemcpy = false;
  // 3. Print the implementation of each function.
  for (auto &F : P) {

    printFunDecl(F.get(), BufStream, C);
    BufStream << " {\n";

    printLocals(F.get(), C, BufStream);

    if (F->Info.IsTask) {
      BufStream << TAB << F->getName() << "_closure *largs = (" << F->getName()
                << "_closure*)(args.get());\n";
    }

    std::unordered_map<const clang::NamedDecl *, std::string> VarRenameMap;
    for (auto &V : F->Vars) {
      if (!V.ASTDecl)
        continue;
      if (V.DeclLoc == IRVarDecl::ARG && F->Info.IsTask)
        VarRenameMap[V.ASTDecl] = "largs->" + GetSym(V.Name);
      else
        VarRenameMap[V.ASTDecl] = GetSym(V.Name);
    }

    auto IRC = IRPrintContext{
        .ASTCtx = C,
        .NewlineSymbol = "\n",
        .GraphVizEscapeChars = false,
        .VarRenames = std::move(VarRenameMap),
        .IdentCB =
            [&](llvm::raw_ostream &Out, IRVarRef VR) {
              switch (VR->DeclLoc) {
              case IRVarDecl::ARG: {
                if (F->Info.IsTask) {
                  Out << "largs->" << GetSym(VR->Name);
                } else {
                  Out << GetSym(VR->Name);
                }
                break;
              }
              case IRVarDecl::LOCAL: {
                Out << GetSym(VR->Name);
                break;
              }
              default:
                PANIC("unsupported");
              }
            },
        .TaskContinuationKey = F->Info.IsTask ? std::string("largs->k") : ""};
    Cilk1EmuPrinter Printer(BufStream, IRC, FnDataStruct);
    Printer.traverse(*F);
    UsesMemcpy |= Printer.UsesMemcpy;
    if (F->Info.IsTask && !Printer.LastReturnPrinted) {
      BufStream << "    return;\n";
    } else if (!Printer.LastReturnPrinted && F->Info.RootFun &&
               !F->Info.RootFun->getReturnType()->isVoidType()) {
      auto RetTy = F->Info.RootFun->getReturnType();
      if (RetTy->isIntegerType())
        BufStream << "    return 0;\n";
      else
        BufStream << "    return {};\n";
    }
    BufStream << "}\n";
  }

  out << "#include \"cilk_explicit.hh\"\n";
  if (UsesMemcpy)
    out << "#include <cstring>\n";
  out << BufStream.str();
}