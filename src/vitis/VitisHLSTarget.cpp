#include "vitis/VitisHLSTarget.hpp"
#include "core/IR.hpp"
#include "vitis/DeepStateDataflow.hpp"
#include "clang/AST/Type.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <map>

#include "core/OpenCilk2IR.hpp"

#define ALIGN(x, A) ((x + A - 1) & -(A))
#define PADDING(x, A) (ALIGN(x, A) - x)

// ─── Constructor ─────────────────────────────────────────────────────────────

VitisHLSTarget::VitisHLSTarget(IRProgram &P, const std::string &AppName,
                               const HardCilkAnalysisResult &Analysis,
                               DriverCallersTy DriverCallers)
    : P(P), AppName(AppName), TaskInfos(Analysis.TaskInfos),
      DriverCallers(std::move(DriverCallers)) {}

// ─── Print-only Type Helpers ─────────────────────────────────────────────────

static const char *printHardCilkType(HardCilkBaseType Ty) {
  switch (Ty) {
  case TY_UINT8:   return "uint8_t";
  case TY_UINT16:  return "uint16_t";
  case TY_UINT32:  return "uint32_t";
  case TY_UINT64:  return "uint64_t";
  case TY_ADDR:    return "addr_t";
  case TY_VOID:    return "void";
  case TY_FLOAT32: return "float";
  case TY_FLOAT64: return "double";
  default:         return nullptr;
  }
}

static llvm::raw_ostream &printHardCilkType(llvm::raw_ostream &Out,
                                            HardCilkType *Ty,
                                            bool Short = false) {
  if (auto *BTy = hctGetIf<HardCilkBaseType>(Ty)) {
    Out << printHardCilkType(*BTy);
  } else if (auto *RTy = hctGetIf<HardCilkRecordType>(Ty)) {
    if (!Short)
      Out << "struct ";
    Out << RTy->Name;
  } else {
    PANIC("Cannot print HardCilk array type without a declarator");
  }
  return Out;
}

static llvm::raw_ostream &printHardCilkDecl(llvm::raw_ostream &Out,
                                            HardCilkType *Ty,
                                            llvm::StringRef Name,
                                            bool Short = false,
                                            bool AddRef = false) {
  if (auto *ATy = hctGetIf<HardCilkArrayType>(Ty)) {
    if (AddRef)
      PANIC("Cannot print reference declarator for HardCilk array type");
    printHardCilkDecl(Out, ATy->Elem.get(), Name, Short, false);
    Out << "[" << ATy->Count << "]";
    return Out;
  }
  printHardCilkType(Out, Ty, Short);
  if (AddRef)
    Out << " &";
  Out << " " << Name;
  return Out;
}

// ─── Code-generation Templates ───────────────────────────────────────────────

const char *DESCRIPTOR_TEMPLATE = R"(#pragma once
#include <cstdint>
#include <cstring>
#include <stddef.h>
#include <stdint.h>

#define MEM_OUT(mem_port, addr, type, value) \
  *((type(*))((uint8_t *)(mem_port) + (addr))) = (value)
#define MEM_IN(mem_port, addr, type) \
  *((type(*))((uint8_t *)(mem_port) + (addr)))

#define MEM_ARR_OUT(mem_port, addr, idx, type, value) \
  *((type(*))((uint8_t *)(mem_port) + (addr) + (idx) * sizeof(type))) = (value)
#define MEM_ARR_IN(mem_port, addr, idx, type) \
  *((type(*))((uint8_t *)(mem_port) + (addr) + (idx) * sizeof(type)))

#define MEM_STRUCT(mem_port, str, str_type, field) \
    (((str_type*)((uint8_t*)(mem_port) + (str)))->field)
#define MEM_STRUCT_ARR_OUT(mem_port, str, str_type, field, idx, type, value) \
  *((type *)((uint8_t *)(mem_port) + (str) + offsetof(str_type, field) +     \
             (idx) * sizeof(type))) = (value)
#define MEM_STRUCT_ARR_IN(mem_port, str, str_type, field, idx, type)         \
  *((type *)((uint8_t *)(mem_port) + (str) + offsetof(str_type, field) +     \
             (idx) * sizeof(type)))

using namespace std;

using addr_t = uint64_t;

// Continuation tag carried in the high 8 bits of a continuation closure address.
// A task that may send its argument to more than one continuation matches this
// against each candidate continuation's <NAME>_TAG to pick the right port.
#define CONT_TAG(cont) ((uint8_t)((cont) >> 56))

)";

#define TAB "  "

// Forward declarations for expression helpers defined later in this file.
void handleArrow(AccessIRExpr *AE, IRPrintContext *C, llvm::raw_ostream &Out);
void handleArray(IndexIRExpr *IE, IRPrintContext *C, llvm::raw_ostream &Out);
void handleDeref(DRefIRExpr *DE, IRPrintContext *C, llvm::raw_ostream &Out);
void handleRef(RefIRExpr *RE, IRPrintContext *C, llvm::raw_ostream &Out);
static void emitHardCilkCast(CastIRExpr *CastE, IRPrintContext *C,
                              llvm::raw_ostream &Out);
static bool emitMemStore(llvm::raw_ostream &Out, IRPrintContext &C,
                         StoreIRStmt *SS);
static const clang::FieldDecl *getAccessFieldDecl(AccessIRExpr *AE);
static std::string getAccessStructName(AccessIRExpr *AE);

// ─── Multi-continuation send-argument helpers ────────────────────────────────

// The defs.h macro that holds a continuation's 8-bit tag, e.g.
// "APPLYFN_REENTRY0_CONT0_TAG".
static std::string contTagMacro(IRFunction *Cont) {
  std::string Name = Cont->getName();
  for (char &c : Name)
    c = (char)std::toupper((unsigned char)c);
  return Name + "_TAG";
}

// Per-destination port names. With a single destination the plain "argOut" /
// "argDataOut" ports are kept; with multiple destinations each gets its own
// "<port>_<contName>" so the task can route by tag.
static std::string argOutPortName(IRFunction *Dst, bool Multi) {
  return Multi ? "argOut_" + Dst->getName() : std::string("argOut");
}
static std::string argDataPortName(IRFunction *Dst, bool Multi) {
  return Multi ? "argDataOut_" + Dst->getName() : std::string("argDataOut");
}

// Send-argument destinations in a stable (name-sorted) order so port lists and
// routing chains are deterministic regardless of pointer-ordered set iteration.
static std::vector<IRFunction *> sortedDests(const HCTaskInfo &Info) {
  std::vector<IRFunction *> Dests(Info.SendArgList.begin(),
                                  Info.SendArgList.end());
  std::sort(Dests.begin(), Dests.end(), [](IRFunction *A, IRFunction *B) {
    return A->getName() < B->getName();
  });
  return Dests;
}

// ─── Helper: callee collection ───────────────────────────────────────────────

static void collectDirectCallees(IRFunction *F,
                                 IRFuncSetTy &Callees) {
  auto visitExpr = [&](auto &&self, IRExpr *E) -> void {
    if (!E)
      return;
    if (auto *CE = dyn_cast<CallIRExpr>(E)) {
      if (auto *FP = std::get_if<IRFunction *>(&CE->Fn))
        Callees.insert(*FP);
      for (auto &Arg : CE->Args)
        self(self, Arg.get());
    } else if (auto *BE = dyn_cast<BinopIRExpr>(E)) {
      self(self, BE->Left.get());
      self(self, BE->Right.get());
    } else if (auto *UE = dyn_cast<UnopIRExpr>(E)) {
      self(self, UE->Expr.get());
    } else if (auto *CE2 = dyn_cast<CastIRExpr>(E)) {
      self(self, CE2->E.get());
    } else if (auto *DE = dyn_cast<DRefIRExpr>(E)) {
      self(self, DE->Expr.get());
    } else if (auto *IE = dyn_cast<IndexIRExpr>(E)) {
      self(self, IE->Arr.get());
      self(self, IE->Ind.get());
    } else if (auto *RE = dyn_cast<RefIRExpr>(E)) {
      self(self, RE->E.get());
    }
  };
  auto visitStmt = [&](IRStmt *S) {
    if (auto *CS = dyn_cast<CopyIRStmt>(S))
      visitExpr(visitExpr, CS->Src.get());
    else if (auto *EW = dyn_cast<ExprWrapIRStmt>(S))
      visitExpr(visitExpr, EW->Expr.get());
    else if (auto *SS = dyn_cast<StoreIRStmt>(S)) {
      visitExpr(visitExpr, SS->Dest.get());
      visitExpr(visitExpr, SS->Src.get());
    } else if (auto *IS = dyn_cast<IfIRStmt>(S))
      visitExpr(visitExpr, IS->Cond.get());
    else if (auto *LS = dyn_cast<LoopIRStmt>(S))
      visitExpr(visitExpr, LS->Cond.get());
    else if (auto *RS = dyn_cast<ReturnIRStmt>(S))
      if (RS->RetVal)
        visitExpr(visitExpr, RS->RetVal.get());
  };
  for (auto &B : *F) {
    for (auto &S : *B)
      visitStmt(S.get());
    if (B->Term)
      visitStmt(B->Term);
  }
}

static IRFuncSetTy computeFuncsNeedingMem(IRProgram &P) {
  IRFuncSetTy Result;
  for (auto &FPtr : P) {
    for (auto &Var : FPtr->Vars) {
      if (Var.DeclLoc != IRVarDecl::ARG)
        continue;
      auto *HCT = clangTypeToHardCilk(Var.Type);
      bool isAddr = false;
      if (auto *BTy = hctGetIf<HardCilkBaseType>(HCT))
        isAddr = (*BTy == TY_ADDR);
      delete HCT;
      if (isAddr) {
        Result.insert(FPtr.get());
        break;
      }
    }
  }
  bool Changed = true;
  while (Changed) {
    Changed = false;
    for (auto &FPtr : P) {
      if (Result.count(FPtr.get()))
        continue;
      IRFuncSetTy Callees;
      collectDirectCallees(FPtr.get(), Callees);
      for (auto *Callee : Callees) {
        if (Result.count(Callee)) {
          Result.insert(FPtr.get());
          Changed = true;
          break;
        }
      }
    }
  }
  return Result;
}

// ─── Inlinable Function Printer ──────────────────────────────────────────────

class HardCilkInlinablePrinter : public ScopedIRTraverser {
private:
  llvm::raw_ostream &Out;
  IRPrintContext &C;
  int IndentLvl = 1;

  llvm::raw_ostream &Indent() {
    for (int i = 0; i < IndentLvl; i++)
      Out << TAB;
    return Out;
  }

  void handleScope(ScopeEvent SE) override {
    switch (SE) {
    case ScopeEvent::Close:
      assert(IndentLvl > 0);
      IndentLvl--;
      Indent() << "}\n";
      break;
    case ScopeEvent::Open:
      Out << " {\n";
      IndentLvl++;
      break;
    case ScopeEvent::Else:
      assert(IndentLvl > 0);
      IndentLvl--;
      Indent() << "} else {\n";
      IndentLvl++;
      break;
    default:
      break;
    }
  }

  void visitStmt(IRStmt *S) {
    if (S->Silent)
      return;
    Indent();
    if (auto *SS = dyn_cast<StoreIRStmt>(S)) {
      if (!emitMemStore(Out, C, SS))
        S->print(Out, C);
    } else {
      S->print(Out, C);
    }
    if (!isa<IfIRStmt>(S) && !isa<LoopIRStmt>(S))
      Out << ";\n";
  }

  void visitBlock(IRBasicBlock *B) override {
    for (auto &S : *B)
      visitStmt(S.get());
    if (B->Term)
      visitStmt(B->Term);
  }

public:
  HardCilkInlinablePrinter(llvm::raw_ostream &Out, IRPrintContext &C)
      : Out(Out), C(C) {}
};

static void
PrintInlinableFunction(llvm::raw_ostream &Out, clang::ASTContext &C,
                       IRFunction *Fn,
                       const IRFuncSetTy &FuncsNeedingMem) {
  bool HasMem = FuncsNeedingMem.count(Fn) > 0;

  bool retIsRef = Fn->getReturnType()->isLValueReferenceType();
  auto *RetHCT = clangTypeToHardCilk(Fn->getReturnType());
  printHardCilkType(Out, RetHCT);
  delete RetHCT;
  if (retIsRef)
    Out << " &";
  Out << " inline " << Fn->getName() << "(";

  bool first = true;
  if (HasMem) {
    Out << "void *mem";
    first = false;
  }
  for (auto &Var : Fn->Vars) {
    if (Var.DeclLoc == IRVarDecl::ARG) {
      if (!first)
        Out << ", ";
      bool isRef = Var.Type->isLValueReferenceType();
      auto *HCT = clangTypeToHardCilk(Var.Type);
      printHardCilkDecl(Out, HCT, GetSym(Var.Name), false, isRef);
      delete HCT;
      first = false;
    }
  }
  Out << ") {\n";

  for (auto &Local : Fn->Vars) {
    if (Local.DeclLoc == IRVarDecl::LOCAL) {
      bool isRef = Local.Type->isLValueReferenceType();
      Out << TAB;
      auto *HCT = clangTypeToHardCilk(Local.Type);
      printHardCilkDecl(Out, HCT, GetSym(Local.Name), false, isRef);
      delete HCT;
      Out << ";\n";
    }
  }

  IRPrintContext IRC{
      .ASTCtx = C,
      .NewlineSymbol = "\n",
      .IdentCB = [](llvm::raw_ostream &Out,
                    IRVarRef VR) { Out << GetSym(VR->Name); },
      .ExprCB =
          [&](IRPrintContext *C, llvm::raw_ostream &Out, IRExpr *E) {
            if (auto *CE = dyn_cast<CallIRExpr>(E)) {
              IRFunction *Callee = nullptr;
              if (auto *FP = std::get_if<IRFunction *>(&CE->Fn))
                Callee = *FP;
              if (Callee)
                Out << Callee->getName();
              else
                Out << std::get<ASTVarRef>(CE->Fn)->getQualifiedNameAsString();
              Out << "(";
              bool firstArg = true;
              if (Callee && FuncsNeedingMem.count(Callee)) {
                Out << "mem";
                firstArg = false;
              }
              for (auto &Arg : CE->Args) {
                if (!firstArg)
                  Out << ", ";
                C->ExprCB(C, Out, Arg.get());
                firstArg = false;
              }
              Out << ")";
            } else if (auto *AE = dyn_cast<AccessIRExpr>(E)) {
              if (AE->Arrow)
                handleArrow(AE, C, Out);
              else
                AE->print(Out, *C);
            } else if (auto *DE = dyn_cast<DRefIRExpr>(E)) {
              handleDeref(DE, C, Out);
            } else if (auto *IE = dyn_cast<IndexIRExpr>(E)) {
              handleArray(IE, C, Out);
            } else if (auto *RE = dyn_cast<RefIRExpr>(E)) {
              handleRef(RE, C, Out);
            } else if (auto *CastE = dyn_cast<CastIRExpr>(E)) {
              emitHardCilkCast(CastE, C, Out);
            } else {
              E->print(Out, *C);
            }
          }};

  HardCilkInlinablePrinter Printer(Out, IRC);
  Printer.traverse(*Fn);
  Out << "}\n\n";
}

// ─── Task Printer Helpers ────────────────────────────────────────────────────

static bool taskHasArgDataOut(const HCTaskInfo &Info) {
  return !typeIsVoid(*Info.RetTy) || Info.GenerateArgOutWriteBuffer;
}

static HardCilkType *getTaskArgDataType(const HCTaskInfo &Info,
                                        HardCilkType &ScratchTy) {
  if (!typeIsVoid(*Info.RetTy))
    return Info.RetTy.get();
  ScratchTy = Info.BufferedArgType;
  return &ScratchTy;
}

// ─── HardCilkPrinter ─────────────────────────────────────────────────────────

class HardCilkPrinter : public ScopedIRTraverser {
private:
  llvm::raw_ostream &Out;
  IRPrintContext &C;
  TaskInfosTy const &TaskInfos;
  int SpawnCtr = 0;
  int ArgDataCtr = 0;
  int IndentLvl = 1;
  std::set<SpawnNextIRStmt *> EmittedSpawnNexts;
  std::map<ClosureDeclIRStmt *, SpawnNextIRStmt *> ClosureToSpawnNext;
  // True while emitting a task whose spawn_next continuation lives INSIDE the
  // same OVERLAP wrapper. Only then is the continuation lowered to an in-order
  // stream push (contStateOut_<cont>) instead of the closure/allocator/
  // spawn_next machinery, and only then do its dependent spawns carry no
  // closure reply address. A task that sits in a wrapper but whose spawn_next
  // is outside it (the one that dependently spawns a real task, handing the
  // loop body off to the scheduler) keeps the ordinary mechanics.
  bool CurTaskOverlap = false;
public:
  // Tasks for which the above holds; filled from the OVERLAP groups.
  IRFuncSetTy StreamingContTasks;

private:

  llvm::raw_ostream &Indent() {
    for (int i = 0; i < IndentLvl; i++)
      Out << TAB;
    return Out;
  }

  void handleScope(ScopeEvent SE) override {
    switch (SE) {
    case ScopeEvent::Close:
      assert(IndentLvl > 0);
      IndentLvl--;
      Indent() << "}\n";
      break;
    case ScopeEvent::Open:
      Out << " {\n";
      IndentLvl++;
      break;
    case ScopeEvent::Else:
      assert(IndentLvl > 0);
      IndentLvl--;
      Indent() << "} else {\n";
      IndentLvl++;
      break;
    default:
      break;
    }
  }

  void handleSpawnNextDecl(ClosureDeclIRStmt *DS, IRFunction *F) {
    const std::string &SpawnNextFnName = DS->Fn->getName();
    const std::string SpawnNextClsName = "SN_" + SpawnNextFnName + "c";
    if (CurTaskOverlap) {
      // In-order streaming: the continuation state is a plain <cont>_task struct
      // that we push onto the contStateOut FIFO. No allocator closure address
      // (closureIn) and no spawn counter — the wrapper pairs each queued state
      // with the in-order memory replies.
      Indent() << SpawnNextFnName << "_task " << SpawnNextClsName << ";\n";
      Indent() << SpawnNextClsName << "._cont = args._cont;\n\n";
      return;
    }
    Indent() << "uint32_t " << SpawnNextClsName << "_cnt = ";
    assert(DS->SpawnCount);
    C.ExprCB(&C, Out, DS->SpawnCount.get());
    Out << ";\n";
    Indent() << SpawnNextFnName << "_task " << SpawnNextClsName << ";\n";
    Indent() << SpawnNextClsName << "._cont = args._cont;\n";
    Indent() << SpawnNextClsName << "._counter = " << SpawnNextClsName
             << "_cnt;\n";
    Indent() << "addr_t " << SpawnNextClsName << "_k = closureIn.read();\n\n";
  }

  void handleSpawnNext(SpawnNextIRStmt *S, IRFunction *F) {
    const std::string &SpawnNextFnName = S->Fn->getName();
    const std::string SpawnNextName = "SN_" + SpawnNextFnName;

    for (auto &[SrcVar, DstVar] : S->Decl->Caller2Callee) {
      if (!SrcVar->IsEphemeral) {
        Indent() << SpawnNextName << "c." << GetSym(DstVar->Name) << " = ";
        C.IdentCB(Out, SrcVar);
        Out << ";\n";
      }
    }

    auto SnInfoIt = TaskInfos.find(S->Fn);
    assert(SnInfoIt != TaskInfos.end());
    auto &SnInfo = SnInfoIt->second;
    const std::string Closure = SpawnNextName + "c"; // SN_<fn>c (assembled closure)

    if (CurTaskOverlap) {
      // Push the fully-populated continuation state onto the in-order FIFO the
      // SystemVerilog merge wrapper reads. The wrapper fills the reply fields
      // (e.g. a_i/b_j) from the ordered memory replies before driving <cont>.
      Indent() << "contStateOut_" << SpawnNextFnName << ".write(" << Closure
               << ");\n\n";
      return;
    }

    unsigned Beats = closureWriteBeats(SnInfo);

    if (Beats == 1) {
      Indent() << SpawnNextFnName << "_spawn_next " << SpawnNextName << ";\n";
      Indent() << SpawnNextName << ".addr = " << Closure << "_k;\n";
      Indent() << SpawnNextName << ".data = " << Closure << ";\n";
      Indent() << SpawnNextName << ".size = " << llvm::Log2_32_Ceil(SnInfo.TaskSize)
               << ";\n";
      Indent() << SpawnNextName << ".allow = " << Closure << "_cnt;\n";
      Indent() << "spawnNext_" << SpawnNextFnName << ".write(" << SpawnNextName
               << ");\n\n";
    } else {
      // Closure is wider than one write-buffer beat: write it as `Beats`
      // ordered beats of `BeatBytes` each. The destination address advances by
      // one beat per write; allow stays 0 until the final beat, which carries
      // the spawn counter to release the continuation.
      unsigned BeatBytes = closureBeatBytes(SnInfo);
      size_t BeatSizeLog = llvm::Log2_32_Ceil(BeatBytes);
      Indent() << "uint8_t *" << SpawnNextName << "_bytes = (uint8_t *)&"
               << Closure << ";\n";
      for (unsigned i = 0; i < Beats; ++i) {
        std::string V = SpawnNextName + std::to_string(i);
        Indent() << SpawnNextFnName << "_spawn_next " << V << ";\n";
        Indent() << V << ".addr = " << Closure << "_k + " << (i * BeatBytes)
                 << ";\n";
        Indent() << "memcpy(" << V << ".data, " << SpawnNextName << "_bytes + "
                 << (i * BeatBytes) << ", " << BeatBytes << ");\n";
        Indent() << V << ".size = " << BeatSizeLog << ";\n";
        Indent() << V << ".allow = "
                 << (i + 1 == Beats ? Closure + "_cnt" : std::string("0"))
                 << ";\n";
        Indent() << "spawnNext_" << SpawnNextFnName << ".write(" << V << ");\n";
      }
      Out << "\n";
    }
  }

  void handleSpawn(ESpawnIRStmt *ES, IRFunction *F) {
    const std::string &SpawnFnName = ES->Fn->getName();
    const std::string SpawnFnArgsName =
        (SpawnFnName + "_args") + std::to_string(SpawnCtr);
    Indent() << SpawnFnName << "_task " << SpawnFnArgsName << ";\n";
    if (ES->SN && CurTaskOverlap) {
      // In-order streaming: the dependent task's reply is matched to the queued
      // continuation state by arrival order in the wrapper, not by a closure
      // reply address, so no _cont is needed.
      Indent() << SpawnFnArgsName << "._cont = 0;\n";
    } else if (ES->SN) {
      const std::string &SpawnNextFnName = ES->SN->Fn->getName();
      const std::string SpawnNextContName = "SN_" + SpawnNextFnName + "c_k";
      if (ES->Dest) {
        assert(ES->Local);
        auto *IdentDest = dyn_cast<IdentIRExpr>(ES->Dest.get());
        assert(IdentDest);
        Indent() << SpawnFnArgsName << "._cont = " << SpawnNextContName
                 << " + offsetof(";
        Out << SpawnNextFnName << "_task, " << GetSym(IdentDest->Ident->Name)
            << ");\n";
      } else {
        Indent() << SpawnFnArgsName << "._cont = " << SpawnNextContName
                 << ";\n";
      }
    } else {
      if (F->Info.IsTask) {
        Indent() << SpawnFnArgsName << "._cont = args._cont;\n";
      } else {
        Indent() << SpawnFnArgsName << "._cont = 0;\n";
      }
    }

    auto DstArgIt = ES->Fn->Vars.begin();
    for (auto &Arg : ES->Args) {
      auto &DstArg = *DstArgIt;
      assert(DstArg.DeclLoc == IRVarDecl::ARG);
      Indent() << SpawnFnArgsName << "." << GetSym(DstArg.Name);
      Out << " = ";
      C.ExprCB(&C, Out, Arg.get());
      Out << ";\n";
      DstArgIt++;
    }

    if (ES->Fn == F) {
      Indent() << "taskOut.write(" << SpawnFnArgsName << ");\n\n";
    } else {
      std::string PortName = "taskGlobalOut_" + ES->Fn->getName();
      if (ES->SN)
        PortName += "_depends_" + ES->SN->Fn->getName();
      Indent() << PortName << ".write(" << SpawnFnArgsName << ");\n\n";
    }
  }

  void handleSendArg(ReturnIRStmt *RS, IRFunction *F) {
    if (F->isVoid() || !RS->RetVal)
      return;
    // Build the writeback packet once; routing only selects which ports the two
    // stream writes target.
    HardCilkType *RetType = clangTypeToHardCilk(F->getReturnType());
    int A = ArgDataCtr++;
    printHardCilkType(Indent(), RetType, true) << "_arg_out a" << A << ";\n";
    Indent() << "a" << A << ".addr = args._cont;\n";
    Indent() << "a" << A << ".data = ";
    C.ExprCB(&C, Out, RS->RetVal.get());
    Out << ";\n";
    size_t RetTypeSz = llvm::Log2_32_Ceil(hardCilkTypeSize(RetType));
    Indent() << "a" << A << ".size = " << RetTypeSz << ";"
             << " // TODO calculation could be wrong fix manually for now\n";
    Indent() << "a" << A << ".allow = 1;\n";
    emitArgRouting(F, [&](const std::string &ArgOut, const std::string &ArgData) {
      Indent() << ArgOut << ".write(args._cont);\n";
      Indent() << ArgData << ".write(a" << A << ");\n";
    });
    delete RetType;
  }

  bool emitBufferedStore(StoreIRStmt *SS, IRFunction *F) {
    auto TaskInfoIt = TaskInfos.find(F);
    assert(TaskInfoIt != TaskInfos.end());
    auto &Info = TaskInfoIt->second;
    auto AllowIt = Info.BufferedStoreAllowMap.find(SS);
    if (AllowIt == Info.BufferedStoreAllowMap.end())
      return false;

    HardCilkType ScratchTy;
    HardCilkType *ArgDataTy = getTaskArgDataType(Info, ScratchTy);
    printHardCilkType(Indent(), ArgDataTy, true)
        << "_arg_out a" << ArgDataCtr << ";\n";
    if (auto *IE = dyn_cast<IndexIRExpr>(SS->Dest.get())) {
      Indent() << "a" << ArgDataCtr << ".addr = ((addr_t)(";
      C.ExprCB(&C, Out, IE->Arr.get());
      Out << ") + (";
      C.ExprCB(&C, Out, IE->Ind.get());
      Out << " * sizeof(" << IE->ArrType.getAsString() << ")));\n";
    } else if (auto *DE = dyn_cast<DRefIRExpr>(SS->Dest.get())) {
      Indent() << "a" << ArgDataCtr << ".addr = (addr_t)(";
      C.ExprCB(&C, Out, DE->Expr.get());
      Out << ");\n";
    } else {
      return false;
    }
    Indent() << "a" << ArgDataCtr << ".data = ";
    C.ExprCB(&C, Out, SS->Src.get());
    Out << ";\n";
    size_t ArgDataTySz = llvm::Log2_32_Ceil(hardCilkTypeSize(ArgDataTy));
    Indent() << "a" << ArgDataCtr << ".size = " << ArgDataTySz << ";\n";
    Indent() << "a" << ArgDataCtr << ".allow = " << AllowIt->second << ";\n";
    int A = ArgDataCtr++;
    emitArgRouting(F, [&](const std::string &, const std::string &ArgData) {
      Indent() << ArgData << ".write(a" << A << ");\n";
    });
    return true;
  }

  // Lower an in-place ++/-- on a memory location to an explicit read-modify-
  // write: `MEM_OUT(mem, a, T, MEM_IN(mem, a, T) +/- 1)`. A bare `MEM_IN(...)++`
  // is not a legal AXI memory access (the location must be read, updated, then
  // written back). Returns true when the statement was a memory inc/dec and has
  // been emitted here. Non-memory operands (locals) fall through to the normal
  // printer. The address subexpression is a simple field/ident ref with no side
  // effects, so evaluating it twice is safe.
  bool emitMemIncDec(ExprWrapIRStmt *EW) {
    auto *UE = dyn_cast<UnopIRExpr>(EW->Expr.get());
    if (!UE)
      return false;
    bool Inc = UE->Op == UnopIRExpr::UNOP_PREINC ||
               UE->Op == UnopIRExpr::UNOP_POSTINC;
    bool Dec = UE->Op == UnopIRExpr::UNOP_PREDEC ||
               UE->Op == UnopIRExpr::UNOP_POSTDEC;
    if (!Inc && !Dec)
      return false;
    const char *Op = Inc ? "+" : "-";

    if (auto *DE = dyn_cast<DRefIRExpr>(UE->Expr.get())) {
      std::string Ty = DE->PointeeType.getAsString();
      Indent() << "MEM_OUT(mem, ";
      C.ExprCB(&C, Out, DE->Expr.get());
      Out << ", " << Ty << ", MEM_IN(mem, ";
      C.ExprCB(&C, Out, DE->Expr.get());
      Out << ", " << Ty << ") " << Op << " 1);\n";
      return true;
    }
    // Plain array element `arr[i]++` (not a struct-field array); struct/arrow
    // array accesses fall through to the default printer.
    if (auto *IE = dyn_cast<IndexIRExpr>(UE->Expr.get());
        IE && !isa<AccessIRExpr>(IE->Arr.get())) {
      std::string Ty = IE->ArrType.getAsString();
      Indent() << "MEM_ARR_OUT(mem, ";
      C.ExprCB(&C, Out, IE->Arr.get());
      Out << ", ";
      C.ExprCB(&C, Out, IE->Ind.get());
      Out << ", " << Ty << ", MEM_ARR_IN(mem, ";
      C.ExprCB(&C, Out, IE->Arr.get());
      Out << ", ";
      C.ExprCB(&C, Out, IE->Ind.get());
      Out << ", " << Ty << ") " << Op << " 1);\n";
      return true;
    }
    return false;
  }

  void visitStmt(IRStmt *S, IRBasicBlock *B) {
    auto *F = B->getParent();
    if (S->Silent)
      return;

    if (auto *ES = dyn_cast<ESpawnIRStmt>(S)) {
      handleSpawn(ES, F);
      SpawnCtr++;
    } else if (auto *SNS = dyn_cast<SpawnNextIRStmt>(S)) {
      if (!EmittedSpawnNexts.count(SNS))
        handleSpawnNext(SNS, F);
    } else if (auto *CDS = dyn_cast<ClosureDeclIRStmt>(S)) {
      handleSpawnNextDecl(CDS, F);
      // Hoist spawnNext.write() before spawns: the scheduler must receive the
      // allow-count before any spawned task can complete and decrement it.
      //
      // Only on the scheduler path. An OVERLAP task streams its continuation
      // state onto a FIFO the wrapper pops in order — no allow-count, nothing
      // that can decrement early — so there is nothing to hoist for, and
      // hoisting actively corrupts the state: the write copies the carried
      // variables at the point it is emitted, so every update the loop body
      // makes between the declaration and the sync is dropped. randomWalk's
      // reentry advances the RNG and sets `done` in the condition of an `if`
      // that sits after the declaration; hoisted, the continuation received the
      // pre-advance seed and done == 0 forever. Emitting it at its own IR
      // position (the block terminator, at the sync) snapshots the right
      // values.
      if (!CurTaskOverlap) {
        auto It = ClosureToSpawnNext.find(CDS);
        if (It != ClosureToSpawnNext.end()) {
          handleSpawnNext(It->second, F);
          EmittedSpawnNexts.insert(It->second);
        }
      }
    } else if (auto *RS = dyn_cast<ReturnIRStmt>(S)) {
      handleSendArg(RS, F);
    } else if (auto *SS = dyn_cast<StoreIRStmt>(S)) {
      if (!emitBufferedStore(SS, F)) {
        Indent();
        if (!emitMemStore(Out, C, SS))
          S->print(Out, C);
        Out << ";\n";
      }
    } else if (auto *EW = dyn_cast<ExprWrapIRStmt>(S);
               EW && emitMemIncDec(EW)) {
      // Handled: an in-place ++/-- on a memory location is lowered to an
      // explicit read-modify-write (a bare `MEM_IN(...)++` is not a valid AXI
      // memory access).
    } else {
      Indent();
      S->print(Out, C);
      if (!isa<IfIRStmt>(S) && !isa<LoopIRStmt>(S))
        Out << ";\n";
    }
  }

  void visitBlock(IRBasicBlock *B) override {
    for (auto &S : *B)
      visitStmt(S.get(), B);
    if (B->Term)
      visitStmt(B->Term, B);
  }

public:
  HardCilkPrinter(llvm::raw_ostream &Out, IRPrintContext &C,
                  TaskInfosTy const &TaskInfos)
      : Out(Out), C(C), TaskInfos(TaskInfos) {}

  // Emit a single zero-sized writeback packet with allow=1. Used when a buffered
  // store lives inside a loop: every real store carries allow=0, and this final
  // flush releases exactly one output packet regardless of iteration count.
  void emitFinalArgOutFlush(IRFunction *F) {
    auto TaskInfoIt = TaskInfos.find(F);
    assert(TaskInfoIt != TaskInfos.end());
    auto &Info = TaskInfoIt->second;
    HardCilkType ScratchTy;
    HardCilkType *ArgDataTy = getTaskArgDataType(Info, ScratchTy);
    printHardCilkType(Indent(), ArgDataTy, true)
        << "_arg_out a" << ArgDataCtr << ";\n";
    Indent() << "a" << ArgDataCtr << ".addr = 0x3FFFFFFFF;\n";
    Indent() << "a" << ArgDataCtr << ".data = 0;\n";
    Indent() << "a" << ArgDataCtr << ".size = 0;\n";
    Indent() << "a" << ArgDataCtr << ".allow = 1;\n";
    int A = ArgDataCtr++;
    emitArgRouting(F, [&](const std::string &, const std::string &ArgData) {
      Indent() << ArgData << ".write(a" << A << ");\n";
    });
  }

  IRPrintContext &getPrintContext() { return C; }

  // Emit a chosen subset of a task's statements, in the given order. The
  // deep-state dataflow rewrite reprints the original body split across the
  // generated stage functions, so it needs to drive statement emission itself
  // instead of traversing the whole CFG. visitStmt uses its block argument only
  // to reach the parent function, so any block of the task will do.
  void emitStmtSubset(IRFunction *Task, const std::vector<IRStmt *> &Stmts) {
    if (Task->begin() == Task->end())
      return;
    IRBasicBlock *Any = Task->begin()->get();
    for (IRStmt *S : Stmts)
      visitStmt(S, Any);
  }

  void prepareForTask(IRFunction *Task) {
    EmittedSpawnNexts.clear();
    ClosureToSpawnNext.clear();
    CurTaskOverlap = StreamingContTasks.count(Task) > 0;
    for (auto &B : *Task) {
      if (!B->Term)
        continue;
      if (auto *SNS = dyn_cast<SpawnNextIRStmt>(B->Term))
        if (SNS->Decl)
          ClosureToSpawnNext[SNS->Decl] = SNS;
    }
  }

  // Emit `Body(argOutPort, argDataPort)` for the task's send-argument
  // destination(s). With zero/one destination it is a single direct emit on the
  // plain "argOut"/"argDataOut" ports. With several it reads the continuation
  // tag once and emits an if/else-if chain routing to each destination's own
  // "<port>_<contName>" ports.
  void emitArgRouting(
      IRFunction *F,
      const std::function<void(const std::string &, const std::string &)>
          &Body) {
    auto It = TaskInfos.find(F);
    assert(It != TaskInfos.end());
    const HCTaskInfo &Info = It->second;
    if (Info.SendArgList.size() <= 1) {
      Body("argOut", "argDataOut");
      return;
    }
    // `_cont_tag` is declared once per PE (see PrintHardCilkTask) so multiple
    // routed sites in the same task don't redeclare it.
    bool First = true;
    for (IRFunction *Dst : sortedDests(Info)) {
      Indent() << (First ? "if" : "} else if") << " (_cont_tag == "
               << contTagMacro(Dst) << ") {\n";
      First = false;
      IndentLvl++;
      Body(argOutPortName(Dst, true), argDataPortName(Dst, true));
      IndentLvl--;
    }
    Indent() << "}\n";
  }

  // Void/terminal completion: forward args._cont to the continuation's argOut,
  // routed by tag when there is more than one destination.
  void emitCompletionArgOut(IRFunction *F) {
    emitArgRouting(F, [&](const std::string &ArgOut, const std::string &) {
      Indent() << ArgOut << ".write(args._cont);\n";
    });
  }
};

// Returns true only if the task body directly emits a MEM_* access or calls an
// inlinable function that does. Having addr_t-typed arguments is not sufficient
// — those are passed through as values in the task struct without dereferencing.
static bool taskNeedsMem(IRFunction *Task,
                          const IRFuncSetTy &FuncsNeedingMem) {
  auto checkExpr = [&FuncsNeedingMem](auto &&self, IRExpr *E) -> bool {
    if (!E)
      return false;
    if (isa<DRefIRExpr>(E) || isa<IndexIRExpr>(E))
      return true;
    if (auto *AE = dyn_cast<AccessIRExpr>(E))
      return AE->Arrow;
    if (auto *CE = dyn_cast<CallIRExpr>(E)) {
      if (auto *FP = std::get_if<IRFunction *>(&CE->Fn))
        if (FuncsNeedingMem.count(*FP))
          return true;
      for (auto &Arg : CE->Args)
        if (self(self, Arg.get()))
          return true;
      return false;
    }
    if (auto *BE = dyn_cast<BinopIRExpr>(E))
      return self(self, BE->Left.get()) || self(self, BE->Right.get());
    if (auto *UE = dyn_cast<UnopIRExpr>(E))
      return self(self, UE->Expr.get());
    if (auto *CE2 = dyn_cast<CastIRExpr>(E))
      return self(self, CE2->E.get());
    if (auto *RE = dyn_cast<RefIRExpr>(E))
      return self(self, RE->E.get());
    return false;
  };
  auto checkStmt = [&](IRStmt *S) -> bool {
    if (auto *CS = dyn_cast<CopyIRStmt>(S))
      return checkExpr(checkExpr, CS->Src.get());
    if (auto *EW = dyn_cast<ExprWrapIRStmt>(S))
      return checkExpr(checkExpr, EW->Expr.get());
    if (auto *SS = dyn_cast<StoreIRStmt>(S))
      return checkExpr(checkExpr, SS->Dest.get()) ||
             checkExpr(checkExpr, SS->Src.get());
    if (auto *IS = dyn_cast<IfIRStmt>(S))
      return checkExpr(checkExpr, IS->Cond.get());
    if (auto *LS = dyn_cast<LoopIRStmt>(S))
      return checkExpr(checkExpr, LS->Cond.get());
    if (auto *RS = dyn_cast<ReturnIRStmt>(S))
      return checkExpr(checkExpr, RS->RetVal.get());
    if (auto *ES = dyn_cast<ESpawnIRStmt>(S)) {
      for (auto &Arg : ES->Args)
        if (checkExpr(checkExpr, Arg.get()))
          return true;
      return false;
    }
    return false;
  };
  for (auto &B : *Task) {
    for (auto &S : *B)
      if (checkStmt(S.get()))
        return true;
    if (B->Term && checkStmt(B->Term))
      return true;
  }
  return false;
}

// ─── Memory channel planning ─────────────────────────────────────────────────
//
// All reads of a PE sharing one m_axi port serialize on that port: the
// flushable pipeline cannot overlap iteration i's read request with iteration
// i-1's outstanding response, and a dependent-load chain (read v, then read
// pGraph[v]) pushes the II up to the full AXI round-trip latency (observed
// II=144 on triangleDAE's applyFn_reentry0). Emitting one `void *mem_<i>`
// port per static read site on the SAME `gmem` bundle with a distinct
// `channel = <i>` gives each site its own AXI ID, so the HLS scheduler treats
// them as independent ports while the RTL still exposes the single
// m_axi_gmem interface (no port explosion; the per-PE s_axi_control offset
// registers stay tied idle at 0 exactly as with the single `mem` port). The
// dependent-load latency then moves into pipeline depth and the II returns
// to 1 (verified on applyFn_reentry0: II 144 → 1, Vitis HLS 2022.1).
//
// Hazard rule: AXI orders transactions only within one ID, so splitting is
// legal only when reordering cannot be observed. We split only tasks whose
// m_axi traffic is read-only — reads commute freely regardless of aliasing —
// and keep the single ordered `mem` port whenever the task writes memory
// through m_axi (a MEM_*_OUT store or a ++/-- lowered to read-modify-write),
// calls an inlinable helper that touches memory (helpers keep the plain
// `mem` parameter), or contains any construct this planner does not
// understand. Buffered stores (BufferedStoreAllowMap) leave through the
// argDataOut write-buffer stream, not this PE's m_axi port, so they do not
// block splitting.
namespace {
struct MemChannelPlan {
  bool Active = false;      // true while printing a split task's body
  unsigned NumChannels = 0; // number of mem_<i> ports when Active
  std::map<const IRExpr *, unsigned> Site; // read-site node → channel
  // Every read site in emission order, and whether the task was provably
  // read-only. Recorded even when the split is declined (a single read site
  // gains nothing from a channel of its own) because the deep-state dataflow
  // rewrite needs the site list regardless of how many there are.
  std::vector<const IRExpr *> Sites;
  bool Blocked = false;
};
// Plan for the task currently being printed. Inlinable functions and
// non-split tasks print with Active=false and keep the plain `mem` name.
MemChannelPlan GMemPlan;

// ─── Deep-state dataflow rewrite: printer redirections ───────────────────────
//
// When a task is emitted as a DATAFLOW region (see DeepStateDataflow.hpp) its
// original statements are reprinted inside the generated stage functions, where
// two things no longer mean what they meant in the flat body:
//
//   * a read site is not a MEM_* macro any more -- the value arrived on a FIFO,
//     so the site prints as the local that holds it;
//   * a variable may live in a different stage than the one being printed, so it
//     prints as the local that carries it across (_c_<v>, _r_<v>).
//
// Both are empty while printing anything else, so every other task and every
// inlinable function is byte-for-byte unaffected.
std::map<const IRExpr *, std::string> DFSiteValue;
IRVarMapTy<std::string> DFIdentRedirect;

// Print the local holding `Site`'s loaded value, if the site has been lifted
// into a load stage. Returns false when the site should print as a MEM_* macro.
bool printDFSiteValue(llvm::raw_ostream &Out, const IRExpr *Site) {
  auto It = DFSiteValue.find(Site);
  if (It == DFSiteValue.end())
    return false;
  Out << It->second;
  return true;
}
} // namespace

// Channels are AXI IDs on one physical interface, so this cap does not
// multiply RTL ports; it only bounds the per-ID bookkeeping in the HLS m_axi
// adapter. Extra read sites wrap around — reads may share an ID safely, it
// only costs II.
static constexpr unsigned MaxMemChannels = 16;

// Print the m_axi port name a MEM_* read at `Site` must use.
static void printMemReadPort(llvm::raw_ostream &Out, const IRExpr *Site) {
  if (!GMemPlan.Active) {
    Out << "mem";
    return;
  }
  auto It = GMemPlan.Site.find(Site);
  // A miss means the planner did not see a node the printer emits; channel 0
  // always exists and reads are order-free, so falling back is safe, just
  // suboptimal.
  Out << "mem_" << (It == GMemPlan.Site.end() ? 0u : It->second);
}

static MemChannelPlan
planMemChannels(IRFunction *Task, const HCTaskInfo &Info,
                const IRFuncSetTy &FuncsNeedingMem) {
  MemChannelPlan Plan;
  bool Blocked = false;
  std::vector<const IRExpr *> ReadSites;

  // Mirrors the print-time ExprCB (and handleArrow/handleArray/handleDeref/
  // handleRef): records every node emitted as a MEM read macro and blocks the
  // split on anything not provably read-only.
  std::function<void(IRExpr *)> walkExpr = [&](IRExpr *E) {
    if (!E || Blocked)
      return;
    if (auto *CE = dyn_cast<CallIRExpr>(E)) {
      if (auto *FP = std::get_if<IRFunction *>(&CE->Fn))
        if (FuncsNeedingMem.count(*FP)) {
          Blocked = true;
          return;
        }
      for (auto &Arg : CE->Args)
        walkExpr(Arg.get());
    } else if (auto *AE = dyn_cast<AccessIRExpr>(E)) {
      if (AE->Arrow)
        ReadSites.push_back(AE); // MEM_STRUCT
    } else if (auto *DE = dyn_cast<DRefIRExpr>(E)) {
      ReadSites.push_back(DE); // MEM_IN
      walkExpr(DE->Expr.get());
    } else if (auto *IE = dyn_cast<IndexIRExpr>(E)) {
      // MEM_ARR_IN, or MEM_STRUCT_ARR_IN when the base is an arrow access to
      // an array field — in that case handleArray prints the base as a plain
      // ident, so its arrow node is not a separate read site.
      bool StructArr = false;
      if (auto *BAE = dyn_cast<AccessIRExpr>(IE->Arr.get()); BAE && BAE->Arrow)
        if (auto *Field = getAccessFieldDecl(BAE);
            Field && Field->getType()->isArrayType())
          StructArr = true;
      ReadSites.push_back(IE);
      if (!StructArr)
        walkExpr(IE->Arr.get());
      walkExpr(IE->Ind.get());
    } else if (auto *RE = dyn_cast<RefIRExpr>(E)) {
      // handleRef prints an address computation: an Index base is decomposed
      // into Arr/Ind with no MEM access of its own.
      if (auto *IE2 = dyn_cast<IndexIRExpr>(RE->E.get())) {
        walkExpr(IE2->Arr.get());
        walkExpr(IE2->Ind.get());
      } else {
        walkExpr(RE->E.get());
      }
    } else if (auto *CastE = dyn_cast<CastIRExpr>(E)) {
      walkExpr(CastE->E.get());
    } else if (auto *BE = dyn_cast<BinopIRExpr>(E)) {
      walkExpr(BE->Left.get());
      walkExpr(BE->Right.get());
    } else if (auto *UE = dyn_cast<UnopIRExpr>(E)) {
      bool IncDec = UE->Op == UnopIRExpr::UNOP_PREINC ||
                    UE->Op == UnopIRExpr::UNOP_POSTINC ||
                    UE->Op == UnopIRExpr::UNOP_PREDEC ||
                    UE->Op == UnopIRExpr::UNOP_POSTDEC;
      bool MemLval = isa<DRefIRExpr>(UE->Expr.get()) ||
                     isa<IndexIRExpr>(UE->Expr.get());
      if (auto *OAE = dyn_cast<AccessIRExpr>(UE->Expr.get()))
        MemLval |= OAE->Arrow;
      if (IncDec && MemLval) {
        Blocked = true; // read-modify-write on memory
        return;
      }
      walkExpr(UE->Expr.get());
    } else if (isa<IdentIRExpr>(E) || isa<ASTLiteralIRExpr>(E) ||
               isa<IntLiteralIRExpr>(E) || isa<FIdentIRExpr>(E)) {
      // no memory access
    } else {
      Blocked = true; // unknown expression: cannot prove read-only
    }
  };

  // Mirrors HardCilkPrinter::visitStmt / PrintHardCilkTask emission.
  std::function<void(IRStmt *)> walkStmt = [&](IRStmt *S) {
    if (!S || S->Silent || Blocked)
      return;
    if (auto *ES = dyn_cast<ESpawnIRStmt>(S)) {
      for (auto &Arg : ES->Args)
        walkExpr(Arg.get());
    } else if (isa<SpawnNextIRStmt>(S)) {
      // handleSpawnNext copies closure fields from plain idents only.
    } else if (auto *CDS = dyn_cast<ClosureDeclIRStmt>(S)) {
      // handleSpawnNextDecl prints SpawnCount except on the OVERLAP path.
      if (!Info.IsOverlap && CDS->SpawnCount)
        walkExpr(CDS->SpawnCount.get());
    } else if (auto *RS = dyn_cast<ReturnIRStmt>(S)) {
      if (!Task->isVoid() && RS->RetVal)
        walkExpr(RS->RetVal.get());
    } else if (auto *SS = dyn_cast<StoreIRStmt>(S)) {
      if (Info.BufferedStoreAllowMap.count(SS)) {
        // emitBufferedStore: address/data go out the argDataOut stream; only
        // the subexpressions are printed, never a MEM_*_OUT.
        if (auto *IE = dyn_cast<IndexIRExpr>(SS->Dest.get())) {
          walkExpr(IE->Arr.get());
          walkExpr(IE->Ind.get());
        } else if (auto *DE = dyn_cast<DRefIRExpr>(SS->Dest.get())) {
          walkExpr(DE->Expr.get());
        } else {
          // emitBufferedStore only handles Index/DRef dests; anything else
          // falls back to plain printing, where a non-ident lvalue would be
          // an m_axi write.
          if (!isa<IdentIRExpr>(SS->Dest.get())) {
            Blocked = true;
            return;
          }
        }
        walkExpr(SS->Src.get());
      } else if (isa<IdentIRExpr>(SS->Dest.get())) {
        // Plain local assignment; only the source can read memory.
        walkExpr(SS->Src.get());
      } else {
        // emitMemStore (Index/DRef dest) emits a MEM_*_OUT, and any other
        // lvalue (e.g. an arrow access) prints as `MEM_STRUCT(...) = v` — an
        // m_axi write on this port either way.
        Blocked = true;
      }
    } else if (auto *EW = dyn_cast<ExprWrapIRStmt>(S)) {
      walkExpr(EW->Expr.get()); // a mem ++/-- blocks inside walkExpr
    } else if (auto *CS = dyn_cast<CopyIRStmt>(S)) {
      walkExpr(CS->Src.get());
    } else if (auto *IS = dyn_cast<IfIRStmt>(S)) {
      walkExpr(IS->Cond.get());
    } else if (auto *LS = dyn_cast<LoopIRStmt>(S)) {
      walkStmt(LS->Init);
      walkExpr(LS->Cond.get());
      walkStmt(LS->Inc);
    } else if (isa<SyncIRStmt>(S) || isa<BreakIRStmt>(S) ||
               isa<ContinueIRStmt>(S) || isa<ScopeAnnotIRStmt>(S)) {
      // no expressions
    } else {
      Blocked = true; // ASTStmtWrap or new kinds: cannot prove read-only
    }
  };

  for (auto &B : *Task) {
    for (auto &S : *B)
      walkStmt(S.get());
    if (B->Term)
      walkStmt(B->Term);
  }

  Plan.Blocked = Blocked;
  if (!Blocked)
    Plan.Sites = ReadSites;
  // A single read site gains nothing from a channel of its own.
  if (Blocked || ReadSites.size() < 2)
    return Plan;
  Plan.Active = true;
  Plan.NumChannels =
      std::min<unsigned>((unsigned)ReadSites.size(), MaxMemChannels);
  for (unsigned I = 0; I < ReadSites.size(); ++I)
    Plan.Site[ReadSites[I]] = I % MaxMemChannels;
  return Plan;
}

// ─── Deep-state dataflow emission ────────────────────────────────────────────
//
// Emits the stage functions for a rewritten PE. The top-level function itself is
// still printed by PrintHardCilkTask: only its body changes, from the flat
// pipeline to a DATAFLOW region wiring these stages together.
//
// The original statements are reprinted here, split across stages, with two
// redirections active (see DFSiteValue / DFIdentRedirect):
//   * a lifted read site prints as the local holding its FIFO value;
//   * a variable that lives in another stage prints as the local carrying it.
//
// EVERY STAGE BODY IS A FINITE FUNCTION -- one read from each input stream, one
// write to each output stream, then return. It is tempting to wrap it in
// `for (;;)` so the process is visibly free-running like the PE it replaces;
// doing so DEADLOCKS the region. HLS gates a downstream dataflow process on a
// `start_for_<stage>` FIFO whose write enable is the UPSTREAM process's
// `ap_ready` -- one start token per completed upstream invocation, not one token
// at reset. A stage that never returns never emits one, so merge's ap_start
// never rises and df_ctx is never read. The free-running behaviour comes from
// HLS re-invoking the finite process, not from a loop in the source.
//
// `#pragma HLS PIPELINE II = 1` therefore stays as the FIRST STATEMENT OF THE
// FUNCTION BODY, where it is a function pipeline rather than a loop pipeline.
// Without it a stage is re-invoked only after the previous invocation retires,
// i.e. its II equals its latency (measured: HLS_SYN_TPT 26 instead of 1, and
// half the throughput).

// Tunables, set once from the command line.
static deepstate::Opts DFOpts;

// Byte address of a read site, exactly as the MEM_* macro would compute it:
//   MEM_ARR_IN(m,a,i,T) == MEM_IN(m, (uint64_t)(a) + (uint64_t)(i)*sizeof(T), T)
// so the issue stage and the original code address the same byte.
static void printDFAddr(llvm::raw_ostream &Out, IRPrintContext &IRC,
                        const IRExpr *Site) {
  if (auto *IE = dyn_cast<IndexIRExpr>(Site)) {
    Out << "((uint64_t)(";
    IRC.ExprCB(&IRC, Out, IE->Arr.get());
    Out << ") + (uint64_t)(";
    IRC.ExprCB(&IRC, Out, IE->Ind.get());
    Out << ") * sizeof(" << IE->ArrType.getAsString() << "))";
    return;
  }
  auto *DE = dyn_cast<DRefIRExpr>(Site);
  assert(DE && "deep-state read site is neither MEM_ARR_IN nor MEM_IN");
  Out << "((uint64_t)(";
  IRC.ExprCB(&IRC, Out, DE->Expr.get());
  Out << "))";
}

// The m_axi port serving a site. Unchanged from the flat emission, so the
// rewrite neither adds nor removes channels: the synthesized
// C_M_AXI_GMEM_ID_WIDTH stays put, and a wrapper built against the old width
// still matches (a mismatch there hangs on the first read rather than failing
// to elaborate).
static std::string dfMemPort(unsigned Chan) {
  return GMemPlan.Active ? ("mem_" + std::to_string(Chan)) : std::string("mem");
}

void VitisHLSTarget::setDeepStateMode(int M) {
  DFOpts.M = static_cast<deepstate::Mode>(M);
}

static void PrintDeepStateStages(
    llvm::raw_ostream &Out, clang::ASTContext &C, IRPrintContext &IRC,
    HardCilkPrinter &Printer, IRFunction *Task, HCTaskInfo &Info,
    const deepstate::Plan &DF,
    const std::vector<std::pair<std::string, std::string>> &Intfs,
    bool IsTerminalCont) {
  const std::string N = Task->getName();
  const std::string TaskTy = Intfs[0].second;
  const std::string Depth = N + "_DF_DEPTH";

  Out << "// " << N << " is emitted as a DATAFLOW region: the m_axi latency is\n"
      << "// crossed by 64-bit addresses instead of by the whole task closure,\n"
      << "// which waits in a BRAM FIFO (df_ctx) and rejoins at the last stage.\n"
      << "// A flushable pipeline would have registered roughly "
      << DF.StateBitsBefore << " bits of closure\n"
      << "// state (estimated depth " << DF.EstDepth << ").\n";
  Out << "#define " << Depth << " " << DFOpts.Inflight << "\n\n";

  // ── issue ──────────────────────────────────────────────────────────────────
  Out << "static void " << N << "_df_issue(\n";
  Out << "  hls::stream<" << TaskTy << "> &taskIn";
  for (auto &S : DF.Sites)
    Out << ",\n  hls::stream<uint64_t> &" << S.AddrStream;
  for (auto &X : DF.Carry)
    Out << ",\n  hls::stream<" << X.CTy << "> &" << X.Stream;
  Out << ",\n  hls::stream<" << TaskTy << "> &df_ctx\n) {\n";
  Out << "#pragma HLS PIPELINE II = 1\n";
  Out << "    " << TaskTy << " args = taskIn.read();\n";
  for (auto &S : DF.Sites) {
    Out << "    " << S.AddrStream << ".write(";
    printDFAddr(Out, IRC, S.E);
    Out << ");\n";
  }
  for (auto &X : DF.Carry)
    Out << "    " << X.Stream << ".write(args." << GetSym(X.Var->Name) << ");\n";
  Out << "    df_ctx.write(args);\n";
  Out << "}\n\n";

  // ── one load per read site ─────────────────────────────────────────────────
  // Carries nothing but the address, so its depth costs ~64 bits a stage
  // whatever the closure width is. This is where the memory latency lives.
  for (unsigned K = 0; K < DF.Sites.size(); ++K) {
    const auto &S = DF.Sites[K];
    const std::string Port = dfMemPort(S.Chan);
    Out << "static void " << N << "_df_load" << K << "(\n";
    Out << "  void *" << Port << ",\n";
    Out << "  hls::stream<uint64_t> &" << S.AddrStream << ",\n";
    Out << "  hls::stream<" << S.ElemTy << "> &" << S.DataStream << "\n) {\n";
    Out << "#pragma HLS PIPELINE II = 1\n";
    Out << "    uint64_t _addr = " << S.AddrStream << ".read();\n";
    Out << "    " << S.DataStream << ".write(MEM_IN(" << Port << ", _addr, "
        << S.ElemTy << "));\n";
    Out << "}\n\n";
  }

  // ── compute ────────────────────────────────────────────────────────────────
  // Only the narrow values reach here, so the arithmetic latency costs a few
  // hundred bits rather than a few tens of thousands.
  if (DF.hasCompute()) {
    Out << "static void " << N << "_df_compute(\n";
    bool First = true;
    auto Sep = [&]() {
      if (!First)
        Out << ",\n";
      First = false;
    };
    for (auto &X : DF.Carry) {
      Sep();
      Out << "  hls::stream<" << X.CTy << "> &" << X.Stream;
    }
    for (auto &S : DF.Sites)
      if (S.InCompute) {
        Sep();
        Out << "  hls::stream<" << S.ElemTy << "> &" << S.DataStream;
      }
    for (auto &X : DF.Result) {
      Sep();
      Out << "  hls::stream<" << X.CTy << "> &" << X.Stream;
    }
    Out << "\n) {\n";
    Out << "#pragma HLS PIPELINE II = 1\n";
    for (auto &X : DF.Carry)
      Out << "    " << X.CTy << " " << X.Local << " = " << X.Stream
          << ".read();\n";
    for (auto &S : DF.Sites)
      if (S.InCompute)
        Out << "    " << S.ElemTy << " " << S.ValueVar << " = " << S.DataStream
            << ".read();\n";

    // Print each definition by hand rather than through the printer: the
    // destination and the incoming value of a mutated ARG field are the same
    // IRVarRef (`args.contributions = args.contributions + ...`), so the two
    // sides need different names and a single redirection map cannot give
    // them that. Emitting the left-hand side here and only redirecting while
    // the right-hand side prints keeps them apart, and adding the redirection
    // afterwards makes a later statement see the new value.
    DFSiteValue.clear();
    DFIdentRedirect.clear();
    for (auto &S : DF.Sites)
      if (S.InCompute)
        DFSiteValue[S.E] = S.ValueVar;
    for (auto &X : DF.Carry)
      DFIdentRedirect[X.Var] = X.Local;
    for (auto &[V, Val] : DF.TrivialLoadVar)
      DFIdentRedirect[V] = Val;
    for (auto &Step : DF.Compute) {
      Out << "    " << Step.CTy << " " << Step.Local << " = ";
      IRC.ExprCB(&IRC, Out, deepstate::srcExpr(Step.S));
      Out << ";\n";
      DFIdentRedirect[Step.Def] = Step.Local;
    }
    for (auto &X : DF.Result)
      Out << "    " << X.Stream << ".write(" << X.Local << ");\n";
    DFSiteValue.clear();
    DFIdentRedirect.clear();
    Out << "}\n\n";
  }

  // ── merge ──────────────────────────────────────────────────────────────────
  // Reads the closure back out of BRAM and emits the outgoing task(s). Nothing
  // deep happens here, so the wide struct crosses only a stage or two.
  Out << "static void " << N << "_df_merge(\n";
  Out << "  hls::stream<" << TaskTy << "> &df_ctx";
  for (auto &S : DF.Sites)
    if (!S.InCompute)
      Out << ",\n  hls::stream<" << S.ElemTy << "> &" << S.DataStream;
  for (auto &X : DF.Result)
    Out << ",\n  hls::stream<" << X.CTy << "> &" << X.Stream;
  for (unsigned I = 1; I < Intfs.size(); ++I)
    Out << ",\n  hls::stream<" << Intfs[I].second << "> &" << Intfs[I].first;
  Out << "\n) {\n";
  Out << "#pragma HLS PIPELINE II = 1\n";
  Out << "    " << TaskTy << " args = df_ctx.read();\n";
  for (auto &S : DF.Sites)
    if (!S.InCompute)
      Out << "    " << S.ElemTy << " " << S.ValueVar << " = " << S.DataStream
          << ".read();\n";
  for (auto &X : DF.Result)
    Out << "    " << X.CTy << " " << X.Local << " = " << X.Stream
        << ".read();\n";

  DFSiteValue.clear();
  DFIdentRedirect.clear();
  for (auto &S : DF.Sites)
    if (!S.InCompute)
      DFSiteValue[S.E] = S.ValueVar;
  for (auto &[V, Val] : DF.TrivialLoadVar)
    DFIdentRedirect[V] = Val;
  for (auto &X : DF.Result)
    DFIdentRedirect[X.Var] = X.Local;

  // Locals the merge statements still need in their own right. A local that has
  // been redirected is already declared above as the stage's own value.
  //
  // Zero-initialised, unlike the flat emission's equivalent declarations. The
  // split can leave a local that the original straight-line body assigned in a
  // part of the task that now lives upstream, so merge reads it before writing
  // it (pageRank's `v` and `prNextValue`: the closure fields built from them
  // are dead — the OVERLAP wrapper overwrites `v` from the memory reply and
  // nothing consumes `prNextValue` — so the result is bit-exact either way).
  // Dead or not, reading an uninitialised local is undefined behaviour in C
  // simulation and puts X's into RTL simulation, which then propagate through
  // any waveform-based debugging of the PE. Initialising costs nothing: the
  // value is overwritten or unused.
  for (auto &Local : Task->Vars) {
    if (Local.DeclLoc != IRVarDecl::LOCAL)
      continue;
    if (DFIdentRedirect.count(&Local))
      continue;
    Out << "    ";
    auto *HCT = clangTypeToHardCilk(Local.Type);
    printHardCilkDecl(Out, HCT, GetSym(Local.Name));
    // `= 0` for the scalars this covers in practice; `= {}` for a record or an
    // array, where `= 0` would not compile.
    Out << (hctGetIf<HardCilkBaseType>(HCT) ? " = 0" : " = {}");
    delete HCT;
    Out << ";\n";
  }
  if (Info.SendArgList.size() > 1)
    Out << "    uint8_t _cont_tag = CONT_TAG(args._cont);\n";
  Out << "\n";

  Printer.prepareForTask(Task);
  Printer.emitStmtSubset(Task, DF.MergeStmts);
  if ((Info.SendArgList.size() > 0 && Task->isVoid() &&
       Task->Info.SpawnNextList.empty() && Task->Info.SpawnList.empty()) ||
      IsTerminalCont) {
    if (Info.EmitFinalArgOutFlush)
      Printer.emitFinalArgOutFlush(Task);
    Printer.emitCompletionArgOut(Task);
  }
  DFSiteValue.clear();
  DFIdentRedirect.clear();
  Out << "}\n\n";
  (void)C;
}

// Extra m_axi directives for a deep-state PE. `latency` is only a scheduling
// hint, but the right value depends on which shape the PE ended up in, and the
// two shapes want OPPOSITE values -- hence the two Opts fields:
//
//   * flushable pipeline (Mode::Latency, or a Dataflow PE that failed a
//     precondition): the declared latency IS the pipeline depth, and every
//     stage of it registers the whole closure, so small is the point.
//   * DATAFLOW region: the load process registers only a 64-bit address, so
//     depth is nearly free there, and that depth is exactly what lets the
//     schedule absorb a reply that arrives later than declared. Declaring 16
//     against real memory latencies of 24/61 made throughput track memory
//     latency; 64 makes it latency-independent and ~10x higher. See
//     Opts::DFMAxiLatency.
//
// Deliberately NOT max_read_burst_length = 1. Every access here is a single
// scalar, so the burst length only sizes burst-splitting logic that never
// engages -- but 1 makes NUM_BEAT_WIDTH = clog2(1) = 0 in the generated m_axi
// adapter, which then contains `{NUM_BEAT_WIDTH{1'b1}}`. Vivado accepts that
// zero-replication; Verilator rejects it (%Error-ZEROREPL), which takes the
// SystemC regression harness out of service for no gain.
static void printDFMAxiTuning(llvm::raw_ostream &Out,
                              const deepstate::Plan &DF) {
  if (!DF.ApplyLatency)
    return;
  Out << " latency = "
      << (DF.Apply ? DFOpts.DFMAxiLatency : DFOpts.MAxiLatency);
  // The extra outstanding credits only pay for themselves when the loads run as
  // their own dataflow processes; the flushable pipeline has one read in flight
  // per stage regardless, so leave its credits at the config_interface default.
  if (DF.Apply)
    Out << " num_read_outstanding = " << DFOpts.Inflight;
}

// The rewritten top-level body: declare the channels and start the stages.
// Channels must be hls::stream, not arrays -- with ap_ctrl_none HLS cannot
// ping-pong a dataflow channel, so a plain array is either rejected or silently
// serialised. Only df_ctx is forced into BRAM; the rest are narrow enough that
// the default (SRL/LUTRAM) is cheaper.
static void PrintDeepStateBody(
    llvm::raw_ostream &Out, IRFunction *Task, HCTaskInfo &Info,
    const deepstate::Plan &DF,
    const std::vector<std::pair<std::string, std::string>> &Intfs) {
  const std::string N = Task->getName();
  const std::string TaskTy = Intfs[0].second;
  const std::string Depth = N + "_DF_DEPTH";

  Out << "#pragma HLS DATAFLOW\n\n";

  std::vector<std::pair<std::string, std::string>> Chans; // name, type
  for (auto &S : DF.Sites) {
    Chans.push_back({S.AddrStream, "uint64_t"});
    Chans.push_back({S.DataStream, S.ElemTy});
  }
  for (auto &X : DF.Carry)
    Chans.push_back({X.Stream, X.CTy});
  for (auto &X : DF.Result)
    Chans.push_back({X.Stream, X.CTy});
  Chans.push_back({"df_ctx", TaskTy});

  for (auto &[Name, Ty] : Chans)
    Out << "  static hls::stream<" << Ty << "> " << Name << "(\"" << Name
        << "\");\n";
  for (auto &[Name, Ty] : Chans) {
    (void)Ty;
    Out << "#pragma HLS STREAM variable = " << Name << " depth = " << Depth
        << "\n";
  }
  // The whole point of the rewrite: the wide closure waits in block RAM.
  Out << "#pragma HLS BIND_STORAGE variable = df_ctx type = fifo impl = bram\n";
  Out << "\n";

  Out << "  " << N << "_df_issue(taskIn";
  for (auto &S : DF.Sites)
    Out << ", " << S.AddrStream;
  for (auto &X : DF.Carry)
    Out << ", " << X.Stream;
  Out << ", df_ctx);\n";

  for (unsigned K = 0; K < DF.Sites.size(); ++K)
    Out << "  " << N << "_df_load" << K << "(" << dfMemPort(DF.Sites[K].Chan)
        << ", " << DF.Sites[K].AddrStream << ", " << DF.Sites[K].DataStream
        << ");\n";

  if (DF.hasCompute()) {
    Out << "  " << N << "_df_compute(";
    bool First = true;
    auto Sep = [&]() {
      if (!First)
        Out << ", ";
      First = false;
    };
    for (auto &X : DF.Carry) {
      Sep();
      Out << X.Stream;
    }
    for (auto &S : DF.Sites)
      if (S.InCompute) {
        Sep();
        Out << S.DataStream;
      }
    for (auto &X : DF.Result) {
      Sep();
      Out << X.Stream;
    }
    Out << ");\n";
  }

  Out << "  " << N << "_df_merge(df_ctx";
  for (auto &S : DF.Sites)
    if (!S.InCompute)
      Out << ", " << S.DataStream;
  for (auto &X : DF.Result)
    Out << ", " << X.Stream;
  for (unsigned I = 1; I < Intfs.size(); ++I)
    Out << ", " << Intfs[I].first;
  Out << ");\n";

  Out << "}\n\n";
  (void)Info;
}

static void PrintHardCilkTask(llvm::raw_ostream &Out, clang::ASTContext &C,
                              HardCilkPrinter &Printer, IRFunction *Task,
                              HCTaskInfo &Info,
                              const IRFuncSetTy &FuncsNeedingMem,
                              const OverlapGroup *Grp,
                              const IRFuncSetTy &CacheableReaders,
                              std::map<std::string, unsigned> &DFDepths) {
  // A terminal continuation has no send destinations, no spawn_next, and no
  // tail-spawn; it signals program completion by forwarding args._cont to the
  // host. A continuation that tail-spawns another task (e.g. an OVERLAP loop's
  // continuation spawning the reentry again) forwards _cont through that task
  // struct instead, so it is not terminal and must not get an argOut port.
  bool IsTerminalCont = Info.IsCont && Info.SendArgList.empty() &&
                        Task->Info.SpawnNextList.empty() &&
                        Task->Info.SpawnList.empty();

  std::vector<std::pair<std::string, std::string>> intfs;
  // The signature is buffered rather than printed here: the deep-state dataflow
  // rewrite has to emit its stage functions ahead of the top-level function, and
  // it cannot decide whether to until the interface list and the memory plan are
  // known.
  std::string MemParams;
  intfs.push_back(std::make_pair("taskIn", Task->getName() + "_task"));
  bool HasMem = taskNeedsMem(Task, FuncsNeedingMem);
  GMemPlan = HasMem ? planMemChannels(Task, Info, FuncsNeedingMem)
                    : MemChannelPlan{};
  if (HasMem) {
    llvm::raw_string_ostream MP(MemParams);
    if (GMemPlan.Active)
      for (unsigned I = 0; I < GMemPlan.NumChannels; ++I)
        MP << "  void *mem_" << I << ",\n";
    else
      MP << "  void *mem,\n";
  }
  // Build spawn-target → spawn_next-function map by scanning ESpawnIRStmts.
  IRFuncMapTy<IRFunction *> SpawnSNMap;
  for (auto &B : *Task)
    for (auto &S : *B)
      if (auto *ES = dyn_cast<ESpawnIRStmt>(S.get()))
        if (ES->Fn != Task && !SpawnSNMap.count(ES->Fn))
          SpawnSNMap[ES->Fn] = ES->SN ? ES->SN->Fn : nullptr;

  for (IRFunction *SpawnTask : Task->Info.SpawnList) {
    if (SpawnTask == Task) {
      intfs.push_back(std::make_pair("taskOut", Task->getName() + "_task"));
    } else {
      std::string PortName = "taskGlobalOut_" + SpawnTask->getName();
      auto It = SpawnSNMap.find(SpawnTask);
      if (It != SpawnSNMap.end() && It->second)
        PortName += "_depends_" + It->second->getName();
      intfs.push_back(std::make_pair(PortName, SpawnTask->getName() + "_task"));
    }
  }
  // argOut/argDataOut ports are emitted only for leaf senders — tasks that
  // complete here rather than tail-spawning. A task with a non-empty spawn or
  // spawn_next list forwards its argument through the spawned closure and never
  // writes argOut/argDataOut, so emitting those ports would create dead outputs
  // (which Vitis HLS then synthesizes as stray input ports). This matches the
  // JSON sendArgumentList, which is gated on the same empty-spawn-lists rule.
  bool NeedsVoidSend =
      Task->Info.SpawnNextList.empty() && Task->Info.SpawnList.empty();
  if (Info.SendArgList.size() > 0 && NeedsVoidSend) {
    bool NeedsArgData =
        !typeIsVoid(*Info.RetTy) || Info.GenerateArgOutWriteBuffer;
    std::string ArgDataOutTy;
    if (NeedsArgData) {
      llvm::raw_string_ostream ArgDataOutTyS(ArgDataOutTy);
      HardCilkType ScratchTy;
      printHardCilkType(ArgDataOutTyS, getTaskArgDataType(Info, ScratchTy),
                        true);
      ArgDataOutTy += "_arg_out";
    }
    // One destination keeps the plain argOut/argDataOut ports; multiple
    // destinations get a dedicated port pair each, selected at runtime by the
    // continuation tag (see emitArgRouting).
    bool Multi = Info.SendArgList.size() > 1;
    if (!Multi) {
      intfs.push_back(std::make_pair("argOut", "uint64_t"));
      if (NeedsArgData)
        intfs.push_back(std::make_pair("argDataOut", ArgDataOutTy));
    } else {
      for (IRFunction *Dst : sortedDests(Info)) {
        intfs.push_back(std::make_pair(argOutPortName(Dst, true), "uint64_t"));
        if (NeedsArgData)
          intfs.push_back(
              std::make_pair(argDataPortName(Dst, true), ArgDataOutTy));
      }
    }
  }
  if (IsTerminalCont)
    intfs.push_back(std::make_pair("argOut", "uint64_t"));
  if (Task->Info.SpawnNextList.size() > 0) {
    if (Task->Info.SpawnNextList.size() > 1){
      PANIC("UNSUPPORTED: more than one spawn next in a function");
    }
    auto &SNDest = *Task->Info.SpawnNextList.begin();
    if (Grp && Grp->Internal.count(SNDest)) {
      // OVERLAP reentry: push continuation state onto a single in-order FIFO
      // that the generated SystemVerilog wrapper drains — no closure allocator
      // (closureIn) and no spawn_next packet port.
      intfs.push_back(std::make_pair("contStateOut_" + SNDest->getName(),
                                     SNDest->getName() + "_task"));
    } else {
      intfs.push_back(std::make_pair("closureIn", "uint64_t"));
      intfs.push_back(std::make_pair("spawnNext_" + SNDest->getName(),
                                     SNDest->getName() + "_spawn_next"));
    }
  }

  // Vitis HLS aborts building the synthesis data model when a port name is too
  // long: `SsdmCdfg.cpp: INTERNAL_ERROR: Port name change`, preceded by a
  // rename message whose "from" and "to" names are identical. It costs a full
  // csynth run to discover, and the name is assembled from task names the user
  // chose, so warn here instead. Measured on triangleDAE: 68 characters
  // synthesises, 80 does not — the true limit is somewhere between, so flag
  // anything past the longest length known to work.
  static constexpr size_t MaxKnownGoodPortName = 68;
  for (auto &[intfName, intfTy] : intfs)
    if (intfName.size() > MaxKnownGoodPortName)
      llvm::errs() << "warning: port '" << intfName << "' on task '"
                   << Task->getName() << "' is " << intfName.size()
                   << " characters; Vitis HLS has been observed to fail with "
                      "INTERNAL_ERROR (Port name change) on names this long. "
                      "Shorten the task or continuation names.\n";

  // Deep-state dataflow: decide before anything is printed, so the stage
  // functions can go out ahead of the top-level one.
  deepstate::Plan DF;
  // Note that a PE that WRITES m_axi is passed through too. It has no read-site
  // list (planMemChannels stops collecting at the store) so it can never be
  // rewritten into a DATAFLOW region -- plan() re-checks that -- but it does have
  // an m_axi port whose declared `latency` sets its pipeline depth, and hence how
  // many copies of the closure the flushable pipeline registers. Gating the
  // latency hint behind read-only left randomWalk_overlap's three storing applyFn
  // PEs at the 64-cycle default and 169k register bits.
  if (HasMem) {
    unsigned InBits = 8u * (unsigned)(Info.TaskSize + Info.TaskPadding);
    unsigned OutBits = InBits;
    DF = deepstate::plan(Task, GMemPlan.Sites, GMemPlan.Site, InBits, OutBits,
                         DFOpts, /*ReadOnly=*/!GMemPlan.Blocked);
  } else {
    DF.SkipReason = "no m_axi port";
  }
  llvm::errs() << "bombyx: deep-state: " << Task->getName() << ": ";
  if (DF.Apply)
    llvm::errs() << "DATAFLOW rewrite (" << DF.Sites.size() << " load site(s), "
                 << DF.Compute.size() << " compute step(s), ~"
                 << DF.StateBitsBefore << " bits of pipeline state avoided)";
  else if (DF.ApplyLatency)
    llvm::errs() << "m_axi latency = " << DFOpts.MAxiLatency
                 << " (flushable pipeline kept; ~" << DF.StateBitsBefore
                 << " bits of state at the default latency, cut to ~"
                 << (DF.StateBitsBefore / 72 * (DFOpts.MAxiLatency + 8)) << ")";
  else
    llvm::errs() << "unchanged (" << DF.SkipReason << ")";
  llvm::errs() << "\n";

  if (DF.Apply) {
    PrintDeepStateStages(Out, C, Printer.getPrintContext(), Printer, Task, Info,
                         DF, intfs, IsTerminalCont);
    // The one part of the rewrite that cannot be expressed in the source: the
    // start-FIFO depth is a solution-level directive, so it has to reach the
    // per-PE TCL. Same value as <PE>_DF_DEPTH -- the start tokens must not let
    // `issue` run further ahead than the ctx FIFO can hold.
    DFDepths[Task->getName()] = DFOpts.Inflight;
  }

  Out << "void " << Task->getName() << " (\n";
  Out << MemParams;
  bool first = true;
  for (auto &[intfName, intfTy] : intfs) {
    if (!first)
      Out << ",\n";
    Out << "  hls::stream<" << intfTy << "> &" << intfName;
    first = false;
  }
  Out << "\n) {\n\n";

  for (auto &[intfName, _] : intfs)
    Out << "#pragma HLS INTERFACE mode = axis port = " << intfName << "\n";
  if (HasMem && GMemPlan.Active) {
    // Read-only PE: one port per read site, all on the default `gmem` bundle,
    // each with its own AXI ID (`channel`) so the scheduler can overlap their
    // transactions (see planMemChannels).
    for (unsigned I = 0; I < GMemPlan.NumChannels; ++I) {
      Out << "#pragma HLS INTERFACE mode = m_axi port = mem_" << I
          << " bundle = gmem channel = " << I;
      printDFMAxiTuning(Out, DF);
      Out << "\n";
    }
  } else if (HasMem) {
    Out << "#pragma HLS INTERFACE mode = m_axi port = mem";
    printDFMAxiTuning(Out, DF);
    // Intentionally no max_widen_bitwidth. Forcing a wide (256-bit) bus on an
    // OVERLAP sub-PE made Vitis HLS emit read-data realignment logic keyed on
    // wide-bus address bits while leaving the actual m_axi port at its narrow
    // natural width — which returned 0 for unaligned sub-word reads (e.g. a[i]
    // with i>0). Let Vitis infer the natural width; the OVERLAP wrapper's per-PE
    // m_axi data width is reconciled to the synthesized width post-synthesis in
    // build_hls.sh (see PrintVitisHLSArtifacts), so wrapper masters still agree
    // with the collapsed PEs and the HardCilk-parsed port widths.
    (void)Grp;
    Out << "\n";
    // Spatial-reuse cache. This PE reads one address per invocation, and the
    // round that drives it issues addresses inside a single AXI beat back to
    // back (`pGraph[2*v]` then `pGraph[2*v+1]`). A small cache on the port
    // turns the second read into a hit, so the pair costs one memory round
    // trip instead of two — the same saving as merging them into one wide
    // read, with no change to the reply type or the wrapper.
    //
    // Emitted only when the pointer is provably never written anywhere on the
    // FPGA (computeCacheableReaders). A cache over memory some other PE writes
    // would serve stale data — a cross-PE RAW hazard the pragma cannot see.
    // `lines=2` is the whole point (hold the beat across the pair); `depth=32`
    // covers the reads in flight down the reader's in-order channel.
    if (CacheableReaders.count(Task))
      Out << "#pragma HLS cache port = mem lines = 2 depth = 32\n";
  }
  // Every PE runs as a free-running, pipelined kernel: no block-level control
  // protocol and a flushable pipeline so it keeps draining its input streams.
  // This applies to m_axi PEs too — they still need ap_ctrl_none.
  Out << "#pragma HLS INTERFACE ap_ctrl_none port = return\n";
  if (DF.Apply) {
    PrintDeepStateBody(Out, Task, Info, DF, intfs);
    GMemPlan = MemChannelPlan{};
    return;
  }
  Out << "#pragma HLS PIPELINE II = 1 style = flp\n";
  Out << "\n";

  for (auto &Local : Task->Vars) {
    if (Local.DeclLoc == IRVarDecl::LOCAL) {
      Out << TAB;
      auto *HCT = clangTypeToHardCilk(Local.Type);
      printHardCilkDecl(Out, HCT, GetSym(Local.Name));
      delete HCT;
      Out << ";\n";
    }
  }

  Out << "  " << intfs[0].second << " args = taskIn.read();\n\n";
  // Tasks that may send to more than one continuation route every argOut /
  // argDataOut write by this tag; declare it once for all routed sites.
  if (Info.SendArgList.size() > 1)
    Out << "  uint8_t _cont_tag = CONT_TAG(args._cont);\n\n";
  Printer.prepareForTask(Task);
  Printer.traverse(*Task);
  if ((Info.SendArgList.size() > 0 && Task->isVoid() &&
       Task->Info.SpawnNextList.empty() && Task->Info.SpawnList.empty()) ||
      IsTerminalCont) {
    if (Info.EmitFinalArgOutFlush)
      Printer.emitFinalArgOutFlush(Task);
    // Forward args._cont to the completion port, routed by tag when this task
    // can complete into more than one continuation. IsTerminalCont has an empty
    // SendArgList, so it falls through to the single plain argOut.
    Printer.emitCompletionArgOut(Task);
  }
  Out << "}\n\n";
  GMemPlan = MemChannelPlan{}; // deactivate: only this task's body is split
}

// ─── Memory Access Emitters ──────────────────────────────────────────────────

static const clang::FieldDecl *getAccessFieldDecl(AccessIRExpr *AE) {
  IRVarRef SR = AE->getStructVarRef();
  if (!SR)
    return nullptr;
  clang::QualType BaseTy = AE->Arrow ? SR->Type->getPointeeType() : SR->Type;
  // desugar is not available here; use the clang API directly
  while (auto *ET = clang::dyn_cast<clang::ElaboratedType>(BaseTy.getTypePtr()))
    BaseTy = ET->getNamedType();
  auto *RT = BaseTy->getAs<clang::RecordType>();
  if (!RT)
    return nullptr;
  for (auto *Field : RT->getDecl()->fields()) {
    if (Field->getName().str() == AE->Field)
      return Field;
  }
  return nullptr;
}

static std::string getAccessStructName(AccessIRExpr *AE) {
  IRVarRef SR = AE->getStructVarRef();
  assert(SR && "getAccessStructName: non-ident base not yet supported");
  clang::QualType BaseTy = AE->Arrow ? SR->Type->getPointeeType() : SR->Type;
  while (auto *ET = clang::dyn_cast<clang::ElaboratedType>(BaseTy.getTypePtr()))
    BaseTy = ET->getNamedType();
  if (auto *RT = BaseTy->getAs<clang::RecordType>())
    return RT->getDecl()->getName().str();
  std::string StructName = BaseTy.getUnqualifiedType().getAsString();
  if (StructName.rfind("struct ", 0) == 0)
    StructName.erase(0, 7);
  if (StructName.empty())
    PANIC("Could not resolve accessed struct type for '%s'", AE->Field.c_str());
  return StructName;
}

void handleArrow(AccessIRExpr *AE, IRPrintContext *C, llvm::raw_ostream &Out) {
  if (printDFSiteValue(Out, AE))
    return;
  IRVarRef SR = AE->getStructVarRef();
  assert(SR && "handleArrow: non-ident base not yet supported");
  std::string StructName = getAccessStructName(AE);
  Out << "MEM_STRUCT(";
  printMemReadPort(Out, AE);
  Out << ", ";
  C->IdentCB(Out, SR);
  Out << ", " << StructName << ", " << AE->Field << ")";
}

void handleArray(IndexIRExpr *IE, IRPrintContext *C, llvm::raw_ostream &Out) {
  if (printDFSiteValue(Out, IE))
    return;
  if (auto *AE = dyn_cast<AccessIRExpr>(IE->Arr.get())) {
    if (AE->Arrow) {
      if (auto *Field = getAccessFieldDecl(AE);
          Field && Field->getType()->isArrayType()) {
        IRVarRef SR = AE->getStructVarRef();
        assert(SR && "handleArray: non-ident struct base not yet supported");
        Out << "MEM_STRUCT_ARR_IN(";
        printMemReadPort(Out, IE);
        Out << ", ";
        C->IdentCB(Out, SR);
        Out << ", " << getAccessStructName(AE) << ", " << AE->Field << ", ";
        C->ExprCB(C, Out, IE->Ind.get());
        Out << ", " << IE->ArrType.getAsString() << ")";
        return;
      }
    }
  }
  Out << "MEM_ARR_IN(";
  printMemReadPort(Out, IE);
  Out << ", ";
  C->ExprCB(C, Out, IE->Arr.get());
  Out << ", ";
  C->ExprCB(C, Out, IE->Ind.get());
  Out << ", " << IE->ArrType.getAsString() << ")";
}

void handleDeref(DRefIRExpr *DE, IRPrintContext *C, llvm::raw_ostream &Out) {
  if (printDFSiteValue(Out, DE))
    return;
  Out << "MEM_IN(";
  printMemReadPort(Out, DE);
  Out << ", ";
  C->ExprCB(C, Out, DE->Expr.get());
  Out << ", " << DE->PointeeType.getAsString() << ")";
}

static bool emitMemStore(llvm::raw_ostream &Out, IRPrintContext &C,
                         StoreIRStmt *SS) {
  if (auto *IE = dyn_cast<IndexIRExpr>(SS->Dest.get())) {
    if (auto *AE = dyn_cast<AccessIRExpr>(IE->Arr.get())) {
      if (AE->Arrow) {
        if (auto *Field = getAccessFieldDecl(AE);
            Field && Field->getType()->isArrayType()) {
          IRVarRef SR = AE->getStructVarRef();
          assert(SR && "emitMemStore: non-ident struct base not yet supported");
          Out << "MEM_STRUCT_ARR_OUT(mem, ";
          C.IdentCB(Out, SR);
          Out << ", " << getAccessStructName(AE) << ", " << AE->Field << ", ";
          C.ExprCB(&C, Out, IE->Ind.get());
          Out << ", " << IE->ArrType.getAsString() << ", ";
          C.ExprCB(&C, Out, SS->Src.get());
          Out << ")";
          return true;
        }
      }
    }
    Out << "MEM_ARR_OUT(mem, ";
    C.ExprCB(&C, Out, IE->Arr.get());
    Out << ", ";
    C.ExprCB(&C, Out, IE->Ind.get());
    Out << ", " << IE->ArrType.getAsString() << ", ";
    C.ExprCB(&C, Out, SS->Src.get());
    Out << ")";
    return true;
  } else if (auto *DE = dyn_cast<DRefIRExpr>(SS->Dest.get())) {
    Out << "MEM_OUT(mem, ";
    C.ExprCB(&C, Out, DE->Expr.get());
    Out << ", " << DE->PointeeType.getAsString() << ", ";
    C.ExprCB(&C, Out, SS->Src.get());
    Out << ")";
    return true;
  }
  return false;
}

static void emitHardCilkCast(CastIRExpr *CastE, IRPrintContext *C,
                             llvm::raw_ostream &Out) {
  Out << "((";
  if (CastE->getCastType()->isPointerType())
    Out << "uint64_t";
  else
    CastE->getCastType().print(Out, C->ASTCtx.getPrintingPolicy());
  Out << ") ";
  C->ExprCB(C, Out, CastE->E.get());
  Out << ")";
}

void handleRef(RefIRExpr *RE, IRPrintContext *C, llvm::raw_ostream &Out) {
  if (auto *IE = dyn_cast<IndexIRExpr>(RE->E.get())) {
    Out << "(";
    C->ExprCB(C, Out, IE->Arr.get());
    Out << " + (";
    C->ExprCB(C, Out, IE->Ind.get());
    Out << ") * sizeof(" << IE->ArrType.getAsString() << "))";
    return;
  }
  Out << "((addr_t)&";
  C->ExprCB(C, Out, RE->E.get());
  Out << ")";
}

// ─── VitisHLSTarget::PrintHardCilk ───────────────────────────────────────────

void VitisHLSTarget::PrintHardCilk(llvm::raw_ostream &Out,
                                   clang::ASTContext &C) {
  DataflowStartFifoDepth.clear();
  Out << "#include \"hls_stream.h\"\n";
  Out << "#include \"" << AppName << "_defs.h\"\n";
  for (auto &Inc : ExtraIncludes)
    Out << Inc << "\n";
  Out << "\n";

  const IRFuncSetTy FuncsNeedingMem = computeFuncsNeedingMem(P);

  IRPrintContext IRC = IRPrintContext{
      .ASTCtx = C,
      .NewlineSymbol = "\n",
      .IdentCB =
          [&](llvm::raw_ostream &Out, IRVarRef VR) {
            if (auto It = DFIdentRedirect.find(VR);
                It != DFIdentRedirect.end()) {
              Out << It->second;
              return;
            }
            switch (VR->DeclLoc) {
            case IRVarDecl::ARG:
              Out << "args." << GetSym(VR->Name);
              break;
            case IRVarDecl::LOCAL:
              Out << GetSym(VR->Name);
              break;
            default:
              PANIC("unsupported");
            }
          },
      .ExprCB =
          [&](IRPrintContext *C, llvm::raw_ostream &Out, IRExpr *E) {
            if (auto *CE = dyn_cast<CallIRExpr>(E)) {
              IRFunction *Callee = nullptr;
              if (auto *FP = std::get_if<IRFunction *>(&CE->Fn))
                Callee = *FP;
              if (Callee)
                Out << Callee->getName();
              else
                Out << std::get<ASTVarRef>(CE->Fn)->getQualifiedNameAsString();
              Out << "(";
              bool firstArg = true;
              if (Callee && FuncsNeedingMem.count(Callee)) {
                Out << "mem";
                firstArg = false;
              }
              for (auto &Arg : CE->Args) {
                if (!firstArg)
                  Out << ", ";
                C->ExprCB(C, Out, Arg.get());
                firstArg = false;
              }
              Out << ")";
            } else if (auto *AE = dyn_cast<AccessIRExpr>(E)) {
              if (AE->Arrow)
                handleArrow(AE, C, Out);
              else
                AE->print(Out, *C);
            } else if (auto *DE = dyn_cast<DRefIRExpr>(E)) {
              handleDeref(DE, C, Out);
            } else if (auto *IE = dyn_cast<IndexIRExpr>(E)) {
              handleArray(IE, C, Out);
            } else if (auto *RE = dyn_cast<RefIRExpr>(E)) {
              handleRef(RE, C, Out);
            } else if (auto *CastE = dyn_cast<CastIRExpr>(E)) {
              emitHardCilkCast(CastE, C, Out);
            } else {
              E->print(Out, *C);
            }
          }};

  for (auto &FPtr : P) {
    IRFunction *F = FPtr.get();
    if (TaskInfos.find(F) != TaskInfos.end())
      continue;
    if (!F->Info.SpawnList.empty() || !F->Info.SpawnNextList.empty())
      continue;
    if (F->Info.IsTask)
      continue;
    PrintInlinableFunction(Out, C, F, FuncsNeedingMem);
  }

  // Identify the collapsed OVERLAP subsystem: its member AXI PEs are all pinned
  // to one shared bus width so the wrapper can wire their masters together.
  const std::vector<OverlapGroup> OverlapGroups =
      computeOverlapGroups(TaskInfos);

  // Readers that may carry an `#pragma HLS cache` on their m_axi port: spatial
  // reuse proven, and the pointer provably never written on the FPGA.
  const IRFuncSetTy CacheableReaders =
      computeCacheableReaders(P, TaskInfos);

  HardCilkPrinter Printer(Out, IRC, TaskInfos);
  // A task streams its continuation state only when that continuation is
  // collapsed into the same wrapper. The task that hands the loop body off to a
  // real spawned callee keeps the scheduler's closure/allocator mechanics even
  // though it is itself inside the wrapper.
  for (const OverlapGroup &G : OverlapGroups)
    for (IRFunction *T : G.Internal)
      if (!T->Info.SpawnNextList.empty() &&
          G.Internal.count(*T->Info.SpawnNextList.begin()))
        Printer.StreamingContTasks.insert(T);
  for (auto &[T, Info] : TaskInfos) {
    if (Info.IsSynthetic)
      continue;
    PrintHardCilkTask(Out, C, Printer, T, const_cast<HCTaskInfo &>(Info),
                      FuncsNeedingMem, findOverlapGroup(OverlapGroups, T),
                      CacheableReaders, DataflowStartFifoDepth);
  }
}

// ─── VitisHLSTarget::PrintDef / PrintDefs ────────────────────────────────────

void VitisHLSTarget::PrintDef(llvm::raw_ostream &Out, IRFunction *Task,
                              HCTaskInfo &Info) {
  // Publish this continuation's 8-bit routing tag so PEs that send to multiple
  // continuations can match it against CONT_TAG(args._cont).
  if (Info.IsCont)
    Out << "#define " << contTagMacro(Task) << " " << (unsigned)Info.Tag
        << "\n\n";
  Out << "struct __attribute__((packed))" << Task->getName() << "_task {\n";
  if (Info.IsCont) {
    // Counter is the first field. Choose uint32 vs uint64 to minimize the
    // total power-of-2 task width; uint32 always gives <= size so use it.
    Out << "  uint32_t _counter;\n";
  }
  Out << "  addr_t _cont;\n";
  for (auto &Var : Task->Vars) {
    if (Var.DeclLoc == IRVarDecl::ARG) {
      if (Var.Type->isLValueReferenceType()) {
        PANIC("LValueReference type '%s' is not supported as a task spawn or "
              "spawn_next argument (function '%s')",
              Var.Type.getAsString().c_str(), Task->getName().c_str());
      }
      auto HCTy = clangTypeToHardCilk(Var.Type);
      printHardCilkDecl(Out << "  ", HCTy, GetSym(Var.Name)) << ";\n";
      delete HCTy;
    }
  }
  if (Info.TaskPadding > 0)
    Out << "  uint8_t _padding[" << Info.TaskPadding << "];\n";
  Out << "};\n\n";

  if (Info.IsCont) {
    // The write buffer caps a single beat at MAX_CLOSURE_BEAT_BITS. Closures
    // wider than that are written in several ordered beats, so the spawn_next
    // payload becomes a raw beat-sized byte buffer instead of the full closure.
    unsigned Beats = closureWriteBeats(Info);
    size_t DataBytes =
        Beats > 1 ? closureBeatBytes(Info) : Info.TaskSize + Info.TaskPadding;
    size_t SnSize = DataBytes + hardCilkTypeSize(TY_ADDR) +
                    hardCilkTypeSize(TY_UINT32) * 2;
    // Write-buffer packets must be a power-of-2 width (32/64/128/256/...), so
    // pad the spawn_next packet up to the next power of two rather than to a
    // multiple of 32 (which can yield non-power-of-2 widths like 96).
    size_t SnPadding = (size_t)llvm::NextPowerOf2(SnSize - 1) - SnSize;
    Out << "struct " << Task->getName() << "_spawn_next {\n";
    Out << "  addr_t addr;\n";
    if (Beats > 1)
      Out << "  uint8_t data[" << DataBytes << "];\n";
    else
      Out << "  " << Task->getName() << "_task data;\n";
    Out << "  uint32_t size;\n";
    Out << "  uint32_t allow;\n";
    if (SnPadding > 0)
      Out << "  uint8_t _padding[" << SnPadding << "];\n";
    Out << "};\n\n";
  }

  HardCilkType ScratchTy;
  HardCilkType *ArgDataTy = getTaskArgDataType(Info, ScratchTy);
  bool OkBaseType = true;
  if (taskHasArgDataOut(Info)) {
    if (auto *BTy = hctGetIf<HardCilkBaseType>(ArgDataTy))
      OkBaseType = !ArgOutImplList[*BTy] && (*BTy != TY_VOID);
  }
  if (Info.SendArgList.size() != 0 && taskHasArgDataOut(Info) && OkBaseType) {
    printHardCilkType(Out << "struct __attribute__((packed)) ", ArgDataTy, true)
        << "_arg_out {\n";
    Out << "  addr_t addr;\n";
    printHardCilkDecl(Out << "  ", ArgDataTy, "data") << ";\n";
    Out << "  uint32_t size;\n";
    Out << "  uint32_t allow;\n";
    size_t argOutSize = hardCilkTypeSize(ArgDataTy) +
                        hardCilkTypeSize(TY_UINT64) +
                        hardCilkTypeSize(TY_UINT32) * 2;
    // Pad the arg_out write-buffer packet to a power-of-2 width as well.
    size_t argOutPadding =
        (size_t)llvm::NextPowerOf2(argOutSize - 1) - argOutSize;
    if (argOutPadding > 0)
      Out << "  uint8_t _padding[" << argOutPadding << "];\n";
    Out << "};\n\n";
    if (auto BTy = hctGetIf<HardCilkBaseType>(ArgDataTy))
      ArgOutImplList[*BTy] = true;
  }
}

void VitisHLSTarget::PrintDefs(llvm::raw_ostream &Out) {
  Out << DESCRIPTOR_TEMPLATE;
  for (auto *RD : GRecordDecls) {
    HardCilkRecordType HCRT = clangRecordTypeToHardCilk(RD);
    Out << "struct __attribute__((packed)) " << HCRT.Name << " {\n";
    for (auto &field : HCRT.Fields) {
      printHardCilkDecl(Out << "  ", field.Type.get(), field.Name) << ";\n";
    }
    Out << "};\n\n";
  }
  for (auto &[T, Info] : TaskInfos) {
    if (Info.IsSynthetic)
      continue;
    PrintDef(Out, T, const_cast<HCTaskInfo &>(Info));
  }

  // OVERLAP meta-task type hint: a `#pragma BOMBYX OVERLAP` loop is scheduled as
  // a single meta task <AppName>_overlap_wrapper, entered with the loop-entry
  // (root) task's closure. Publish that closure type under the wrapper's name so
  // host / framework code can size and enqueue the meta task without knowing the
  // subsystem's internal PEs.
  for (auto &[T, Info] : TaskInfos) {
    if (Info.IsOverlap && Info.IsRoot) {
      Out << "// Meta-task closure for the '" << AppName
          << "_overlap_wrapper' OVERLAP subsystem: it is the loop-entry task's\n"
          << "// closure type (the first task inside the wrapper).\n";
      Out << "using " << AppName << "_overlap_wrapper_task = " << T->getName()
          << "_task;\n\n";
      break;
    }
  }
}

// ─── VitisHLSTarget::PrintDriver ─────────────────────────────────────────────

void VitisHLSTarget::PrintDriver(llvm::raw_ostream &Out) {
  IRFunction *DF = nullptr;
  IRFunction *DFC = nullptr;
  for (auto &F : P) {
    if (F->getName() == "bombyx_driver") {
      DF = F.get();
      if (DF->Info.SpawnNextList.size() != 1)
        PANIC("Driver should have exactly one continuation.");
      DFC = *DF->Info.SpawnNextList.begin();
      if (!DFC->Info.SpawnNextList.empty() || !DFC->Info.SpawnList.empty())
        PANIC("Driver continuation should not have continuations, or spawn "
              "anything");
    }
  }
  if (!DF || !DFC) {
    PANIC("To print driver code, please include a function named bombyx_driver "
          "with exactly one cilk_sync.");
  }
  assert(DF->Info.SpawnList.size() == 1 &&
         "unimplemented: handle multiple spawn types in driver");
}
