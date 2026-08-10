#include "vitis/VitisHLSTarget.hpp"
#include "core/IR.hpp"
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
    if (ES->SN) {
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
      auto It = ClosureToSpawnNext.find(CDS);
      if (It != ClosureToSpawnNext.end()) {
        handleSpawnNext(It->second, F);
        EmittedSpawnNexts.insert(It->second);
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

  void prepareForTask(IRFunction *Task) {
    EmittedSpawnNexts.clear();
    ClosureToSpawnNext.clear();
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
                          const std::set<IRFunction *> &FuncsNeedingMem) {
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

static void PrintHardCilkTask(llvm::raw_ostream &Out, clang::ASTContext &C,
                              HardCilkPrinter &Printer, IRFunction *Task,
                              HCTaskInfo &Info,
                              const std::set<IRFunction *> &FuncsNeedingMem) {
  // A terminal continuation has no send destinations and no spawn_next; it
  // signals program completion by forwarding args._cont to the host.
  bool IsTerminalCont = Info.IsCont && Info.SendArgList.empty() &&
                        Task->Info.SpawnNextList.empty();

  std::vector<std::pair<std::string, std::string>> intfs;
  Out << "void " << Task->getName() << " (\n";
  intfs.push_back(std::make_pair("taskIn", Task->getName() + "_task"));
  bool HasMem = taskNeedsMem(Task, FuncsNeedingMem);
  if (HasMem)
    Out << "  void *mem,\n";
  // Build spawn-target → spawn_next-function map by scanning ESpawnIRStmts.
  std::map<IRFunction *, IRFunction *> SpawnSNMap;
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
    intfs.push_back(std::make_pair("closureIn", "uint64_t"));
    intfs.push_back(
        std::make_pair("spawnNext_" + SNDest->getName(), SNDest->getName() + "_spawn_next"));
  }

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
  if (HasMem)
    Out << "#pragma HLS INTERFACE mode = m_axi port = mem\n";
  // Every PE runs as a free-running, pipelined kernel: no block-level control
  // protocol and a flushable pipeline so it keeps draining its input streams.
  // This applies to m_axi PEs too — they still need ap_ctrl_none.
  Out << "#pragma HLS INTERFACE ap_ctrl_none port = return\n";
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
  IRVarRef SR = AE->getStructVarRef();
  assert(SR && "handleArrow: non-ident base not yet supported");
  std::string StructName = getAccessStructName(AE);
  Out << "MEM_STRUCT(mem, ";
  C->IdentCB(Out, SR);
  Out << ", " << StructName << ", " << AE->Field << ")";
}

void handleArray(IndexIRExpr *IE, IRPrintContext *C, llvm::raw_ostream &Out) {
  if (auto *AE = dyn_cast<AccessIRExpr>(IE->Arr.get())) {
    if (AE->Arrow) {
      if (auto *Field = getAccessFieldDecl(AE);
          Field && Field->getType()->isArrayType()) {
        IRVarRef SR = AE->getStructVarRef();
        assert(SR && "handleArray: non-ident struct base not yet supported");
        Out << "MEM_STRUCT_ARR_IN(mem, ";
        C->IdentCB(Out, SR);
        Out << ", " << getAccessStructName(AE) << ", " << AE->Field << ", ";
        C->ExprCB(C, Out, IE->Ind.get());
        Out << ", " << IE->ArrType.getAsString() << ")";
        return;
      }
    }
  }
  Out << "MEM_ARR_IN(mem, ";
  C->ExprCB(C, Out, IE->Arr.get());
  Out << ", ";
  C->ExprCB(C, Out, IE->Ind.get());
  Out << ", " << IE->ArrType.getAsString() << ")";
}

void handleDeref(DRefIRExpr *DE, IRPrintContext *C, llvm::raw_ostream &Out) {
  Out << "MEM_IN(mem, ";
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
  Out << "#include \"hls_stream.h\"\n";
  Out << "#include \"" << AppName << "_defs.h\"\n";
  for (auto &Inc : ExtraIncludes)
    Out << Inc << "\n";
  Out << "\n";

  const std::set<IRFunction *> FuncsNeedingMem = computeFuncsNeedingMem(P);

  IRPrintContext IRC = IRPrintContext{
      .ASTCtx = C,
      .NewlineSymbol = "\n",
      .IdentCB =
          [&](llvm::raw_ostream &Out, IRVarRef VR) {
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

  HardCilkPrinter Printer(Out, IRC, TaskInfos);
  for (auto &[T, Info] : TaskInfos) {
    if (Info.IsSynthetic)
      continue;
    PrintHardCilkTask(Out, C, Printer, T, const_cast<HCTaskInfo &>(Info),
                      FuncsNeedingMem);
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
