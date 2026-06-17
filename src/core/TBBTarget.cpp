#include <clang/AST/ASTContext.h>
#include <clang/AST/Type.h>
#include <clang/Frontend/CompilerInstance.h>
#include <llvm/Support/raw_ostream.h>
#include <set>
#include <unordered_map>

#include "core/Cilk1EmuTarget.hpp"
#include "core/IR.hpp"
#include "core/TBBTarget.hpp"
#include "core/util.hpp"
#include "clang/AST/Decl.h"

#define TAB "    "

// The TBB backend prints the same CPS structure as the Cilk1 emulation
// target, but maps it onto the classic TBB task API (TBB <= 2020):
//
//   THREAD(f) + CLOSURE_DEF(f, ...)  ->  class f_task : public tbb::task
//                                        { args...; void *bx_out; execute(); }
//   spawn_next<C> SN(...)            ->  C_task &SN_C =
//                                          *new (allocate_continuation())
//                                          C_task(bx_out);
//                                        (in root functions: an empty_task
//                                         root + SN_C as its child, so the
//                                         root can wait_for_all())
//   SN_BIND(SN, &spk, field)         ->  child constructed with &SN_C.field
//   spawn<C> sp(c)                   ->  allocate_child + tbb::task::spawn
//   SEND_ARGUMENT(largs->k, v)       ->  SEND_ARGUMENT(bx_out, v) (writes
//                                        through the destination pointer);
//                                        join counting is TBB ref counting.
//
// When the spawn count of a continuation is a compile-time constant and no
// live variables have to be copied into the closure at the sync point, the
// children are spawned eagerly with set_ref_count(N) up front (this matches
// hand-written continuation-passing TBB code). Otherwise children are
// collected in a tbb::task_list and released at the sync point, after the
// copies, so a finished child can never race with them.

///////////////////////////////////////
// 1. Declarations: classes, protos //
/////////////////////////////////////

static void printTBBRootFunDecl(IRFunction *F, llvm::raw_ostream &out,
                                clang::ASTContext &C) {
  assert(F->Info.RootFun && "Non-root fun should be a task!");
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
}

static void printTBBTaskClassDef(IRFunction *F, llvm::raw_ostream &out,
                                 clang::ASTContext &C) {
  const std::string &Name = F->getName();
  out << "class " << Name << "_task : public tbb::task {\n";
  out << "public:\n";
  for (auto &Var : F->Vars) {
    if (Var.DeclLoc != IRVarDecl::ARG)
      continue;
    out << TAB;
    Var.Type.print(out, C.getPrintingPolicy(), GetSym(Var.Name));
    out << ";\n";
  }
  out << TAB << "void *bx_out;\n";
  out << TAB << "explicit " << Name << "_task(void *out) : bx_out(out) {}\n";
  out << TAB << "tbb::task *execute() override;\n";
  out << "};\n";
}

// The original source may include OpenCilk headers that do not exist in a
// plain C++/TBB build; drop any include line mentioning cilk.
static std::string stripCilkIncludes(const std::string &Src) {
  std::string Out;
  Out.reserve(Src.size());
  size_t Pos = 0;
  while (Pos < Src.size()) {
    size_t Eol = Src.find('\n', Pos);
    size_t End = (Eol == std::string::npos) ? Src.size() : Eol + 1;
    std::string Line = Src.substr(Pos, End - Pos);
    bool IsInclude = Line.find("#include") != std::string::npos;
    bool MentionsCilk = Line.find("cilk") != std::string::npos;
    if (!(IsInclude && MentionsCilk))
      Out += Line;
    Pos = End;
  }
  return Out;
}

////////////////////////////
// 2. Print IR Functions //
//////////////////////////

// Copies that still have to be written into the spawn_next closure at the
// sync point. Mirrors Cilk1EmuPrinter::handleSpawnNext: ephemeral variables
// that an unconditional child writes directly are skipped.
static std::vector<std::pair<IRVarRef, IRVarRef>>
pendingSNCopies(SpawnNextIRStmt *S, IRFunction *F, IRBasicBlock *SNBlock) {
  std::vector<std::pair<IRVarRef, IRVarRef>> Copies;
  if (!S->Decl)
    return Copies;
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
  for (auto &[SrcVar, DstVar] : S->Decl->Caller2Callee) {
    if (SrcVar->IsEphemeral && UnconditionalEphemeralVars.count(SrcVar))
      continue;
    Copies.push_back({SrcVar, DstVar});
  }
  return Copies;
}

// True if any argument of Fn is pointer-typed. Pointer arguments may point
// into the spawning task's stack frame (e.g. alloca'd scratch buffers), so
// the parent has to stay alive until the child is done.
static bool hasPointerArg(IRFunction *Fn) {
  for (auto &V : Fn->Vars)
    if (V.DeclLoc == IRVarDecl::ARG && V.Type->isPointerType())
      return true;
  return false;
}

class TBBCPSPrinter : public ScopedIRTraverser {
private:
  // How a spawn_next continuation is joined with its children.
  struct SNPlan {
    bool Static = false; // ref count known up front, spawn children eagerly
    int Count = 0;
    // Pointers into the parent frame may flow into the closures; emulate the
    // blocking join of cilk_explicit.hh (spawn_next destructor cilk_syncs)
    // instead of pure continuation passing: the continuation becomes a child
    // of this task and the task wait_for_all()s at the sync point.
    bool Blocking = false;
  };

  llvm::raw_ostream &Out;
  IRPrintContext &C;
  int SpawnCtr = 0;
  int IndentLvl = 1;
  std::set<ClosureDeclIRStmt *> DeclaredClosures;
  std::unordered_map<IRFunction *, SNPlan> Plans;

public:
  bool LastReturnPrinted = false;
  // True while the most recently printed construct is a return; used to
  // skip a redundant trailing "return nullptr;" at the end of execute().
  bool EndsWithReturn = false;
  bool UsesMemcpy = false;

private:
  llvm::raw_ostream &Indent() {
    for (int i = 0; i < IndentLvl; i++)
      Out << TAB;
    return Out;
  }

  void planFunction(IRFunction *F) {
    std::unordered_map<IRFunction *, ClosureDeclIRStmt *> Decls;
    auto noteDecl = [&](ClosureDeclIRStmt *D) {
      if (D)
        Decls[D->Fn] = D;
    };
    std::set<IRFunction *> NeedsCopies;
    std::set<IRFunction *> NeedsBlocking;
    for (auto &B : *F) {
      for (auto &S : *B) {
        if (auto *CDS = dyn_cast<ClosureDeclIRStmt>(S.get()))
          noteDecl(CDS);
        else if (auto *ES = dyn_cast<ESpawnIRStmt>(S.get())) {
          if (ES->SN) {
            noteDecl(ES->SN->Decl);
            if ((ES->Dest && !ES->Local) || hasPointerArg(ES->Fn) ||
                hasPointerArg(ES->SN->Fn))
              NeedsBlocking.insert(ES->SN->Fn);
          }
        }
      }
      if (B->Term) {
        if (auto *SNS = dyn_cast<SpawnNextIRStmt>(B->Term)) {
          noteDecl(SNS->Decl);
          if (!pendingSNCopies(SNS, F, B.get()).empty())
            NeedsCopies.insert(SNS->Fn);
          if (hasPointerArg(SNS->Fn))
            NeedsBlocking.insert(SNS->Fn);
        }
      }
    }
    for (auto &[Fn, D] : Decls) {
      SNPlan P;
      // Loop-dependent spawn count expressions reference values at the
      // declaration point; only trust plain positive literals.
      if (D->SpawnCount && !NeedsCopies.count(Fn))
        if (auto *IL = dyn_cast<IntLiteralIRExpr>(D->SpawnCount.get()))
          if (IL->Lit > 0) {
            P.Static = true;
            P.Count = IL->Lit;
          }
      P.Blocking = NeedsBlocking.count(Fn) > 0;
      Plans[Fn] = P;
    }
  }

  void handleScope(ScopeEvent SE) override {
    if (SE != ScopeEvent::None)
      EndsWithReturn = false;
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
    const std::string &SNName = DS->Fn->getName();
    if (F->Info.IsTask && Plans[DS->Fn].Blocking) {
      // Blocking join: the continuation runs as our child and we will
      // wait_for_all() at the sync point, keeping this frame (and any
      // pointers into it) alive until the whole subtree is done.
      Indent() << "set_ref_count(2);\n";
      Indent() << SNName << "_task &SN_" << SNName
               << " = *new (allocate_child()) " << SNName
               << "_task(bx_out);\n";
    } else if (F->Info.IsTask) {
      // The continuation takes over our parent and our result destination.
      Indent() << SNName << "_task &SN_" << SNName
               << " = *new (allocate_continuation()) " << SNName
               << "_task(bx_out);\n";
    } else {
      // Root functions have no tbb parent; hang the continuation under an
      // empty root task so the function can wait for it to finish.
      Indent() << "tbb::empty_task &SN_" << SNName
               << "_root = *new (tbb::task::allocate_root()) "
                  "tbb::empty_task;\n";
      Indent() << "SN_" << SNName << "_root.set_ref_count(2);\n";
      Indent() << SNName << "_task &SN_" << SNName << " = *new (SN_" << SNName
               << "_root.allocate_child()) " << SNName << "_task(nullptr);\n";
    }
    auto &Plan = Plans[DS->Fn];
    if (Plan.Static) {
      Indent() << "SN_" << SNName << ".set_ref_count(" << Plan.Count << ");\n";
    } else {
      Indent() << "tbb::task_list SN_" << SNName << "_list;\n";
      Indent() << "int SN_" << SNName << "_cnt = 0;\n";
    }
  }

  void handleSpawnNext(SpawnNextIRStmt *S, IRFunction *F,
                       IRBasicBlock *SNBlock) {
    const std::string &SNName = S->Fn->getName();
    for (auto &[SrcVar, DstVar] : pendingSNCopies(S, F, SNBlock)) {
      if (SrcVar->Type->isArrayType()) {
        UsesMemcpy = true;
        Indent() << "std::memcpy(SN_" << SNName << "." << GetSym(DstVar->Name)
                 << ", ";
        C.IdentCB(Out, SrcVar);
        Out << ", sizeof(";
        C.IdentCB(Out, SrcVar);
        Out << "));\n";
      } else {
        Indent() << "SN_" << SNName << "." << GetSym(DstVar->Name) << " = ";
        C.IdentCB(Out, SrcVar);
        Out << ";\n";
      }
    }
    Indent() << "// Original sync was here\n";
    auto &Plan = Plans[S->Fn];
    if (!Plan.Static) {
      // Release the collected children; with no children the continuation
      // is ready right away (a ref count can only be driven to zero by a
      // completing child).
      Indent() << "if (SN_" << SNName << "_cnt > 0) {\n";
      IndentLvl++;
      Indent() << "SN_" << SNName << ".set_ref_count(SN_" << SNName
               << "_cnt);\n";
      Indent() << "tbb::task::spawn(SN_" << SNName << "_list);\n";
      IndentLvl--;
      Indent() << "} else {\n";
      IndentLvl++;
      Indent() << "tbb::task::spawn(SN_" << SNName << ");\n";
      IndentLvl--;
      Indent() << "}\n";
    }
    if (!F->Info.IsTask) {
      Indent() << "SN_" << SNName << "_root.wait_for_all();\n";
      Indent() << "tbb::task::destroy(SN_" << SNName << "_root);\n";
    } else if (Plan.Blocking) {
      Indent() << "wait_for_all();\n";
    }
  }

  void emitSpawnArgList(ESpawnIRStmt *ES) {
    auto DstArgIt = ES->Fn->Vars.begin();
    for (auto &Arg : ES->Args) {
      while (DstArgIt != ES->Fn->Vars.end() &&
             DstArgIt->DeclLoc != IRVarDecl::ARG)
        DstArgIt++;
      assert(DstArgIt != ES->Fn->Vars.end());
      auto &DstArg = *DstArgIt;
      if (DstArg.Type->isArrayType()) {
        UsesMemcpy = true;
        Indent() << "std::memcpy(sp" << SpawnCtr << "."
                 << GetSym(DstArg.Name) << ", ";
        Arg->print(Out, C);
        Out << ", sizeof(sp" << SpawnCtr << "." << GetSym(DstArg.Name)
            << "));\n";
      } else {
        Indent() << "sp" << SpawnCtr << "." << GetSym(DstArg.Name) << " = ";
        Arg->print(Out, C);
        Out << ";\n";
      }
      DstArgIt++;
    }
  }

  void handleSpawn(ESpawnIRStmt *ES, IRFunction *F) {
    const std::string &SpawnFnName = ES->Fn->getName();
    if (ES->SN) {
      if (ES->SN->Decl)
        handleSpawnNextDecl(ES->SN->Decl, F);
      const std::string &SNName = ES->SN->Fn->getName();

      // Result destination: a field of the spawn_next closure (SN_BIND), an
      // external lvalue (SN_BIND_EXT) or nothing (SN_BIND_VOID).
      std::string DestArg = "nullptr";
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
          if (destIsArg)
            DestArg = "&SN_" + SNName + "." + DestName;
        } else {
          std::string Buf;
          llvm::raw_string_ostream BS(Buf);
          BS << "&(";
          ES->Dest->print(BS, C);
          BS << ")";
          DestArg = BS.str();
        }
      }

      Indent() << SpawnFnName << "_task &sp" << SpawnCtr << " = *new (SN_"
               << SNName << ".allocate_child()) " << SpawnFnName << "_task("
               << DestArg << ");\n";
      emitSpawnArgList(ES);
      if (Plans[ES->SN->Fn].Static) {
        Indent() << "tbb::task::spawn(sp" << SpawnCtr << ");\n\n";
      } else {
        Indent() << "SN_" << SNName << "_list.push_back(sp" << SpawnCtr
                 << ");\n";
        Indent() << "++SN_" << SNName << "_cnt;\n\n";
      }
    } else if (F->Info.IsTask && hasPointerArg(ES->Fn)) {
      // Fire-and-forget spawn whose arguments may point into our frame:
      // run it as a child and wait, so the frame stays alive.
      Indent() << "set_ref_count(2);\n";
      Indent() << SpawnFnName << "_task &sp" << SpawnCtr
               << " = *new (allocate_child()) " << SpawnFnName
               << "_task(bx_out);\n";
      emitSpawnArgList(ES);
      Indent() << "spawn_and_wait_for_all(sp" << SpawnCtr << ");\n";
    } else if (F->Info.IsTask) {
      // Fire-and-forget tail spawn: hand our continuation slot (parent and
      // result destination) to the spawned task and get out of the way.
      Indent() << SpawnFnName << "_task &sp" << SpawnCtr
               << " = *new (allocate_continuation()) " << SpawnFnName
               << "_task(bx_out);\n";
      emitSpawnArgList(ES);
      Indent() << "tbb::task::spawn(sp" << SpawnCtr << ");\n";
    } else {
      // Fire-and-forget from a root function: there is no continuation to
      // inherit, run the task graph to completion here.
      Indent() << SpawnFnName << "_task &sp" << SpawnCtr
               << " = *new (tbb::task::allocate_root()) " << SpawnFnName
               << "_task(nullptr);\n";
      emitSpawnArgList(ES);
      Indent() << "tbb::task::spawn_root_and_wait(sp" << SpawnCtr << ");\n";
    }
  }

  void visitStmt(IRStmt *S, IRBasicBlock *B) {
    auto *F = B->getParent();
    if (S->Silent)
      return;
    EndsWithReturn = isa<ReturnIRStmt>(S);

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
          Out << "SEND_ARGUMENT(bx_out, ";
          RS->RetVal->print(Out, C);
          Out << ");\n";
          Indent() << "return nullptr;\n";
        } else {
          // A previous fire-and-forget spawn already inherited our result
          // destination and parent; completion is then the child's job.
          bool inheritedByContinuation = false;
          for (auto &PrevS : *B) {
            if (PrevS.get() == S)
              break;
            if (auto *PES = dyn_cast<ESpawnIRStmt>(PrevS.get()))
              if (!PES->SN)
                inheritedByContinuation = true;
          }
          if (inheritedByContinuation) {
            Out << "return nullptr;\n";
          } else {
            Out << "SEND_ARGUMENT(bx_out, 0);\n";
            Indent() << "return nullptr;\n";
          }
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
  TBBCPSPrinter(llvm::raw_ostream &Out, IRPrintContext &C, IRFunction *F)
      : Out(Out), C(C) {
    planFunction(F);
  }
};

void PrintTBB(IRProgram &P, llvm::raw_ostream &out, clang::ASTContext &C,
              clang::CompilerInstance &CI) {

  std::string Buf;
  llvm::raw_string_ostream BufStream(Buf);

  // 1. Original source split around the first function definition; root
  // functions are removed (they get reprinted in CPS form below).
  std::string PartA, PartB;
  llvm::raw_string_ostream PartAStream(PartA);
  llvm::raw_string_ostream PartBStream(PartB);
  printOriginalSourceSplit(P, PartAStream, PartBStream, C, CI);
  BufStream << stripCilkIncludes(PartAStream.str());

  // 2. Prototypes for root functions and task class definitions.
  for (auto &F : P) {
    if (F->Info.IsTask)
      continue;
    printTBBRootFunDecl(F.get(), BufStream, C);
    BufStream << ";\n";
  }
  BufStream << "\n";
  for (auto &F : P) {
    if (F->Info.IsTask) {
      printTBBTaskClassDef(F.get(), BufStream, C);
    }
  }

  // 3. The rest of the original source (non-root functions).
  BufStream << stripCilkIncludes(PartBStream.str());
  BufStream << "\n";

  bool UsesMemcpy = false;
  // 4. Implementations: execute() bodies for tasks, CPS bodies for roots.
  for (auto &F : P) {
    if (F->Info.IsTask) {
      BufStream << "tbb::task *" << F->getName() << "_task::execute() {\n";
    } else {
      printTBBRootFunDecl(F.get(), BufStream, C);
      BufStream << " {\n";
    }

    printLocals(F.get(), C, BufStream);

    if (!F->Info.IsTask && F->Info.RootFun &&
        F->Info.RootFun->getNameAsString() == "main") {
      BufStream << TAB << "tbb::task_scheduler_init bx_tbb_init;\n";
    }

    std::unordered_map<const clang::NamedDecl *, std::string> VarRenameMap;
    for (auto &V : F->Vars) {
      if (!V.ASTDecl)
        continue;
      VarRenameMap[V.ASTDecl] = GetSym(V.Name);
    }

    auto IRC = IRPrintContext{
        .ASTCtx = C,
        .NewlineSymbol = "\n",
        .GraphVizEscapeChars = false,
        .VarRenames = std::move(VarRenameMap),
        .TaskContinuationKey = F->Info.IsTask ? std::string("bx_out") : "",
        .TaskReturnStmt = "return nullptr;"};
    TBBCPSPrinter Printer(BufStream, IRC, F.get());
    Printer.traverse(*F);
    UsesMemcpy |= Printer.UsesMemcpy;
    if (F->Info.IsTask) {
      // execute() must return on every path.
      if (!Printer.EndsWithReturn)
        BufStream << TAB << "return nullptr;\n";
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

  out << "#include \"tbb_explicit.hh\"\n";
  if (UsesMemcpy)
    out << "#include <cstring>\n";
  out << BufStream.str();
}

//////////////////////////
// 3. CMake generation //
////////////////////////

void PrintTBBCMake(const std::string &AppName, llvm::raw_ostream &Out) {
  Out << "cmake_minimum_required(VERSION 3.16)\n"
      << "\n"
      << "project(" << AppName << " CXX)\n"
      << "\n"
      << "set(CMAKE_CXX_STANDARD 17)\n"
      << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
      << "\n"
      << "set(TBB_ROOT \"/beta/tools/oneTBB\" CACHE PATH \"Classic TBB "
         "root\")\n"
      << "set(TBB_BUILD "
         "\"${TBB_ROOT}/build/"
         "linux_intel64_gcc_cc13.3.0_libc2.39_kernel6.8.12_release\" CACHE "
         "PATH \"Classic TBB build dir\")\n"
      << "\n"
      << "add_executable(" << AppName << " " << AppName << ".cpp)\n"
      << "\n"
      << "target_include_directories(" << AppName << " PRIVATE\n"
      << "    \"${TBB_ROOT}/include\"\n"
      << ")\n"
      << "\n"
      << "target_link_directories(" << AppName << " PRIVATE\n"
      << "    \"${TBB_BUILD}\"\n"
      << ")\n"
      << "\n"
      << "target_compile_options(" << AppName << " PRIVATE\n"
      << "    -fpermissive\n"
      << ")\n"
      << "\n"
      << "target_link_libraries(" << AppName << " PRIVATE\n"
      << "    tbb\n"
      << "    pthread\n"
      << "    dl\n"
      << "    rt\n"
      << ")\n"
      << "\n"
      << "set_target_properties(" << AppName << " PROPERTIES\n"
      << "    BUILD_RPATH \"${TBB_BUILD}\"\n"
      << "    INSTALL_RPATH \"${TBB_BUILD}\"\n"
      << ")\n";
}
