#include "hardcilk/HardCilkAnalysis.hpp"
#include "core/IR.hpp"
#include "clang/AST/Type.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/MathExtras.h"

#include <iostream>

#define ALIGN(x, A) ((x + A - 1) & -(A))
#define PADDING(x, A) (ALIGN(x, A) - x)

// ─── Type Conversion ─────────────────────────────────────────────────────────

bool typeIsVoid(const HardCilkType &Ty) {
  const HardCilkBaseType *BTy = std::get_if<HardCilkBaseType>(&Ty.V);
  return BTy && (*BTy == TY_VOID);
}

HardCilkRecordType clangRecordTypeToHardCilk(const clang::RecordDecl *RD) {
  std::vector<HardCilkRecordField> fields;
  for (auto field : RD->fields()) {
    std::unique_ptr<HardCilkType> HCT(clangTypeToHardCilk(field->getType()));
    fields.push_back(
        HardCilkRecordField{field->getName().str(), std::move(HCT)});
  }
  return HardCilkRecordType{.Name = RD->getName().str(),
                            .Fields = std::move(fields)};
}

static clang::QualType desugar(clang::QualType QT) {
  while (true) {
    const clang::Type *T = QT.getTypePtrOrNull();
    if (!T)
      break;
    if (auto *ET = clang::dyn_cast<clang::ElaboratedType>(T)) {
      QT = ET->getNamedType();
      continue;
    }
    if (auto *TT = clang::dyn_cast<clang::TypedefType>(T)) {
      QT = TT->desugar();
      continue;
    }
    if (auto *AT = clang::dyn_cast<clang::AttributedType>(T)) {
      QT = AT->getEquivalentType();
      continue;
    }
    break;
  }
  return QT;
}

HardCilkType *clangTypeToHardCilk(IRType &Ty) {
  HardCilkType *HCT = new HardCilkType();

  clang::QualType QT = desugar(Ty);

  if (auto *BTy = clang::dyn_cast<clang::BuiltinType>(QT.getTypePtr())) {
    switch (BTy->getKind()) {
    case clang::BuiltinType::Int:
    case clang::BuiltinType::UInt:
      *HCT = TY_UINT32;
      break;
    case clang::BuiltinType::Char8:
    case clang::BuiltinType::UChar:
      *HCT = TY_UINT8;
      break;
    case clang::BuiltinType::Short:
    case clang::BuiltinType::UShort:
      *HCT = TY_UINT16;
      break;
    case clang::BuiltinType::Long:
    case clang::BuiltinType::ULong:
      // LP64 target (64-bit Linux): `long`/`unsigned long` are 64-bit, and
      // `uint64_t`/`size_t` desugar to them. Mapping to 32 bits truncated them.
      *HCT = TY_UINT64;
      break;
    case clang::BuiltinType::LongLong:
    case clang::BuiltinType::ULongLong:
      *HCT = TY_UINT64;
      break;
    case clang::BuiltinType::Void:
      *HCT = TY_VOID;
      break;
    case clang::BuiltinType::Float:
      *HCT = TY_FLOAT32;
      break;
    case clang::BuiltinType::Double:
      *HCT = TY_FLOAT64;
      break;
    default:
      PANIC("Unsupported builtin %d", BTy->getKind());
    }
  } else if (QT->isPointerType()) {
    *HCT = TY_ADDR;
  } else if (auto *AT =
                 clang::dyn_cast<clang::ConstantArrayType>(QT.getTypePtr())) {
    HardCilkArrayType ArrayTy;
    ArrayTy.Count = AT->getSize().getZExtValue();
    ArrayTy.Elem.reset(clangTypeToHardCilk(AT->getElementType()));
    *HCT = std::move(ArrayTy);
  } else if (auto *RT = QT->getAs<clang::RecordType>()) {
    *HCT = clangRecordTypeToHardCilk(RT->getAsRecordDecl());
  } else if (QT->isLValueReferenceType()) {
    clang::QualType PointeeTy =
        QT->getAs<clang::ReferenceType>()->getPointeeType();
    delete HCT;
    return clangTypeToHardCilk(PointeeTy);
  } else {
    PANIC("Unsupported type after desugar: %s (%s)", QT.getAsString().c_str(),
          QT->getTypeClassName());
  }

  return HCT;
}

int hardCilkTypeSize(HardCilkBaseType Ty) {
  switch (Ty) {
  case TY_UINT8:    return 1;
  case TY_UINT16:   return 2;
  case TY_UINT32:   return 4;
  case TY_UINT64:   return 8;
  case TY_ADDR:     return 8;
  case TY_FLOAT32:  return 4;
  case TY_FLOAT64:  return 8;
  default:          return -1;
  }
}

int hardCilkTypeSize(HardCilkType *Ty) {
  if (auto *BTy = hctGetIf<HardCilkBaseType>(Ty)) {
    return hardCilkTypeSize(*BTy);
  } else if (auto *ATy = hctGetIf<HardCilkArrayType>(Ty)) {
    return ATy->Count * hardCilkTypeSize(ATy->Elem.get());
  } else {
    auto &RTy = std::get<HardCilkRecordType>(Ty->V);
    int size = 0;
    for (auto &Field : RTy.Fields) {
      size += hardCilkTypeSize(Field.Type.get());
    }
    DBG { std::cerr << "record size: " << size << "\n"; }
    return size;
  }
}

static uint32_t hardCilkTypeBitWidth(HardCilkBaseType Ty) {
  return hardCilkTypeSize(Ty) * 8;
}

// ─── AXI / Memory-Access Analysis ───────────────────────────────────────────

static void collectDirectCalleesHCA(IRFunction *F,
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

// Functions that have a pointer-type arg, or transitively call one that does.
static IRFuncSetTy computeMemFunctions(IRProgram &P) {
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
      collectDirectCalleesHCA(FPtr.get(), Callees);
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

// True if the task body itself dereferences memory or calls a helper that does.
static bool taskBodyAccessesMem(IRFunction *Task,
                                const IRFuncSetTy &MemFuncs) {
  auto checkExpr = [&](auto &&self, IRExpr *E) -> bool {
    if (!E)
      return false;
    if (isa<DRefIRExpr>(E) || isa<IndexIRExpr>(E))
      return true;
    if (auto *AE = dyn_cast<AccessIRExpr>(E))
      return AE->Arrow;
    if (auto *CE = dyn_cast<CallIRExpr>(E)) {
      if (auto *FP = std::get_if<IRFunction *>(&CE->Fn))
        if (MemFuncs.count(*FP))
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

static void analyzeHasAXI(IRProgram &P, TaskInfosTy &TaskInfos) {
  auto MemFuncs = computeMemFunctions(P);
  for (auto &[Task, Info] : TaskInfos)
    Info.HasAXI = taskBodyAccessesMem(Task, MemFuncs);
}

// ─── Memory Read/Store Helpers ───────────────────────────────────────────────

static bool exprHasMemRead(IRExpr *E, bool CountRoot = true) {
  if (!E)
    return false;
  if (auto *IE = dyn_cast<IndexIRExpr>(E)) {
    if (CountRoot)
      return true;
    return exprHasMemRead(IE->Arr.get()) || exprHasMemRead(IE->Ind.get());
  }
  if (auto *DE = dyn_cast<DRefIRExpr>(E)) {
    if (CountRoot)
      return true;
    return exprHasMemRead(DE->Expr.get());
  }
  if (auto *BE = dyn_cast<BinopIRExpr>(E)) {
    return exprHasMemRead(BE->Left.get()) || exprHasMemRead(BE->Right.get());
  }
  if (auto *UE = dyn_cast<UnopIRExpr>(E)) {
    return exprHasMemRead(UE->Expr.get());
  }
  if (auto *CE = dyn_cast<CastIRExpr>(E)) {
    return exprHasMemRead(CE->E.get());
  }
  if (auto *RE = dyn_cast<RefIRExpr>(E)) {
    return exprHasMemRead(RE->E.get(), false);
  }
  if (auto *Call = dyn_cast<CallIRExpr>(E)) {
    for (auto &Arg : Call->Args) {
      if (exprHasMemRead(Arg.get()))
        return true;
    }
    return false;
  }
  if (auto *Spawn = dyn_cast<ISpawnIRExpr>(E)) {
    for (auto &Arg : Spawn->Args) {
      if (exprHasMemRead(Arg.get()))
        return true;
    }
    return false;
  }
  return false;
}

static bool stmtHasMemRead(IRStmt *S) {
  if (!S)
    return false;
  if (auto *SS = dyn_cast<StoreIRStmt>(S)) {
    return exprHasMemRead(SS->Dest.get(), false) ||
           exprHasMemRead(SS->Src.get());
  }
  if (auto *CS = dyn_cast<CopyIRStmt>(S)) {
    return exprHasMemRead(CS->Src.get());
  }
  if (auto *EW = dyn_cast<ExprWrapIRStmt>(S)) {
    return exprHasMemRead(EW->Expr.get());
  }
  if (auto *IS = dyn_cast<IfIRStmt>(S)) {
    return exprHasMemRead(IS->Cond.get());
  }
  if (auto *LS = dyn_cast<LoopIRStmt>(S)) {
    return exprHasMemRead(LS->Cond.get()) || stmtHasMemRead(LS->Init) ||
           stmtHasMemRead(LS->Inc);
  }
  if (auto *RS = dyn_cast<ReturnIRStmt>(S)) {
    return exprHasMemRead(RS->RetVal.get());
  }
  if (auto *ES = dyn_cast<ESpawnIRStmt>(S)) {
    for (auto &Arg : ES->Args) {
      if (exprHasMemRead(Arg.get()))
        return true;
    }
    return exprHasMemRead(ES->Dest.get(), false);
  }
  if (auto *CDS = dyn_cast<ClosureDeclIRStmt>(S)) {
    return exprHasMemRead(CDS->SpawnCount.get());
  }
  return false;
}

static bool getStoreBufferBaseType(StoreIRStmt *SS, HardCilkBaseType &OutTy) {
  clang::QualType StoreTy;
  if (auto *IE = dyn_cast<IndexIRExpr>(SS->Dest.get())) {
    StoreTy = IE->ArrType;
  } else if (auto *DE = dyn_cast<DRefIRExpr>(SS->Dest.get())) {
    StoreTy = DE->PointeeType;
  } else {
    return false;
  }
  std::unique_ptr<HardCilkType> HCT(clangTypeToHardCilk(StoreTy));
  auto *BTy = hctGetIf<HardCilkBaseType>(HCT.get());
  if (!BTy || *BTy == TY_VOID)
    return false;
  OutTy = *BTy;
  return true;
}

static bool isMemoryStore(StoreIRStmt *SS) {
  return isa<IndexIRExpr>(SS->Dest.get()) || isa<DRefIRExpr>(SS->Dest.get());
}

namespace {
class HardCilkStmtOrderCollector : public ScopedIRTraverser {
public:
  std::vector<IRStmt *> Stmts;
  // Statements that lexically sit inside at least one loop body.
  std::set<const IRStmt *> InLoop;

private:
  // One entry per open scope; true if that scope is a loop body. The traverser
  // emits exactly one Open and one Close per if/loop (Else carries no nesting
  // change), so a stack tracks loop nesting correctly even for ifs inside loops.
  std::vector<bool> ScopeIsLoop;
  int LoopDepth = 0;
  IRBasicBlock *CurrentBlock = nullptr;

  void handleScope(ScopeEvent SE) override {
    if (SE == ScopeEvent::Open) {
      bool IsLoop = CurrentBlock && CurrentBlock->Term &&
                    isa<LoopIRStmt>(CurrentBlock->Term);
      ScopeIsLoop.push_back(IsLoop);
      if (IsLoop)
        LoopDepth++;
    } else if (SE == ScopeEvent::Close) {
      assert(!ScopeIsLoop.empty());
      if (ScopeIsLoop.back())
        LoopDepth--;
      ScopeIsLoop.pop_back();
    }
  }

  void visitBlock(IRBasicBlock *B) override {
    CurrentBlock = B;
    for (auto &S : *B) {
      Stmts.push_back(S.get());
      if (LoopDepth > 0)
        InLoop.insert(S.get());
    }
    if (B->Term) {
      Stmts.push_back(B->Term);
      if (LoopDepth > 0)
        InLoop.insert(B->Term);
    }
  }
};
} // namespace

// ─── Analysis Passes ─────────────────────────────────────────────────────────

static void analyzeSendArguments(IRProgram &P, TaskInfosTy &TaskInfos) {
  bool SomethingHappened = true;

  while (SomethingHappened) {
    SomethingHappened = false;

    for (auto &FPtr : P) {
      IRFunction *F = FPtr.get();
      auto FInfoIt = TaskInfos.find(F);
      IRFuncSetTy ParentSendArgList;
      if (FInfoIt != TaskInfos.end()) {
        ParentSendArgList.insert(FInfoIt->second.SendArgList.begin(),
                                 FInfoIt->second.SendArgList.end());
      }
      for (auto &B : *F) {
        for (auto &S : *B) {
          if (auto *ES = dyn_cast<ESpawnIRStmt>(S.get())) {
            if (!ES->SN) {
              if (TaskInfos.find(ES->Fn) == TaskInfos.end())
                continue;
              auto &ESFInfo = TaskInfos[ES->Fn];
              auto SizeI = ESFInfo.SendArgList.size();
              ESFInfo.SendArgList.insert(ParentSendArgList.begin(),
                                         ParentSendArgList.end());
              if (FInfoIt != TaskInfos.end())
                ESFInfo.SendArgList.insert(F->Info.SpawnNextList.begin(),
                                           F->Info.SpawnNextList.end());
              SomethingHappened =
                  SomethingHappened || (SizeI != ESFInfo.SendArgList.size());
              continue;
            }
            if (FInfoIt == TaskInfos.end())
              continue;
            if (TaskInfos.find(ES->Fn) == TaskInfos.end() ||
                TaskInfos.find(ES->SN->Fn) == TaskInfos.end())
              continue;
            auto &ESFInfo = TaskInfos[ES->Fn];
            auto SizeI = ESFInfo.SendArgList.size();
            ESFInfo.SendArgList.insert(ES->SN->Fn);
            SomethingHappened =
                SomethingHappened || (SizeI != ESFInfo.SendArgList.size());
          }
        }
        SpawnNextIRStmt *SNS = nullptr;
        if (B->Term && (SNS = dyn_cast<SpawnNextIRStmt>(B->Term))) {
          if (TaskInfos.find(SNS->Fn) == TaskInfos.end())
            continue;
          auto &ContInfo = TaskInfos[SNS->Fn];
          auto SizeI = ContInfo.SendArgList.size();
          ContInfo.SendArgList.insert(ParentSendArgList.begin(),
                                      ParentSendArgList.end());
          SomethingHappened =
              SomethingHappened || (SizeI != ContInfo.SendArgList.size());
        }
      }
    }
  }
}

static void analyzeArgOutWriteBuffers(TaskInfosTy &TaskInfos) {
  for (auto &[F, Info] : TaskInfos) {
    Info.GenerateArgOutWriteBuffer = false;
    Info.EmitFinalArgOutFlush = false;
    Info.BufferedArgumentBits = 0;
    Info.BufferedArgType = TY_VOID;
    Info.BufferedStoreAllowMap.clear();

    if (Info.IsSynthetic || Info.SendArgList.empty() ||
        !typeIsVoid(*Info.RetTy))
      continue;

    HardCilkStmtOrderCollector Collector;
    Collector.traverse(*F);

    bool SeenLaterMemRead = false;
    bool DisableTask = false;
    bool SawBufferedStore = false;
    std::vector<StoreIRStmt *> BufferedStores;
    HardCilkBaseType BufferedTy = TY_VOID;

    for (auto It = Collector.Stmts.rbegin(); It != Collector.Stmts.rend();
         ++It) {
      auto *SS = dyn_cast<StoreIRStmt>(*It);
      if (SS && !SeenLaterMemRead && isMemoryStore(SS)) {
        HardCilkBaseType StoreTy = TY_VOID;
        if (!getStoreBufferBaseType(SS, StoreTy)) {
          DisableTask = true;
          break;
        }
        if (!SawBufferedStore) {
          SawBufferedStore = true;
          BufferedTy = StoreTy;
        } else if (BufferedTy != StoreTy) {
          DisableTask = true;
          break;
        }
        BufferedStores.push_back(SS);
      }
      SeenLaterMemRead = SeenLaterMemRead || stmtHasMemRead(*It);
    }

    if (DisableTask || BufferedStores.empty())
      continue;

    Info.GenerateArgOutWriteBuffer = true;
    Info.BufferedArgType = BufferedTy;
    Info.BufferedArgumentBits = hardCilkTypeBitWidth(BufferedTy);

    for (auto *SS : BufferedStores) {
      Info.BufferedStoreAllowMap[SS] = 0;
    }
    // The allow=1 store releases exactly one writeback packet. If that store
    // sits inside a loop, the single statement runs once per iteration and would
    // wrongly release N packets. In that case keep every store at allow=0 and
    // emit a single trailing zero-sized flush packet (allow=1) after the body.
    StoreIRStmt *AllowStore = BufferedStores.front();
    if (Collector.InLoop.count(AllowStore))
      Info.EmitFinalArgOutFlush = true;
    else
      Info.BufferedStoreAllowMap[AllowStore] = 1;
  }
}

// ─── Main Analysis Entry Point ───────────────────────────────────────────────

HardCilkAnalysisResult RunHardCilkAnalysis(IRProgram &P) {
  HardCilkAnalysisResult Result;
  TaskInfosTy &TaskInfos = Result.TaskInfos;

  // Lazily creates the synthetic base continuation inside Result.
  // auto ensureBaseContinuation = [&]() -> IRFunction * {
  //   if (Result.SyntheticBaseContinuation)
  //     return Result.SyntheticBaseContinuation.get();
  //   Result.SyntheticBaseContinuation =
  //       std::make_unique<IRFunction>(0, "base_continuation",
  //                                   clang::QualType(), &P);
  //   auto *BaseCont = Result.SyntheticBaseContinuation.get();
  //   auto &BaseContInfo = TaskInfos[BaseCont];
  //   BaseContInfo.IsCont = true;
  //   BaseContInfo.IsSynthetic = true;
  //   BaseContInfo.RetTy = std::make_unique<HardCilkType>(TY_VOID);
  //   return BaseCont;
  // };

  // Pass 1: register every espawn target as a task.
  for (auto &F : P) {
    for (auto &G : F->Info.SpawnList) {
      if (TaskInfos.find(G) == TaskInfos.end())
        TaskInfos[G] = HCTaskInfo();
      TaskInfos[G].IsRoot |= !F->Info.IsTask;
    }
  }

  // Pass 2: register spawnNext continuations only for functions already in
  // TaskInfos (i.e., reachable as tasks). Entry-point functions are excluded to
  // avoid dead duplicate tasks.
  for (auto &F : P) {
    if (TaskInfos.find(F.get()) == TaskInfos.end())
      continue;
    for (auto &G : F->Info.SpawnNextList) {
      if (TaskInfos.find(G) == TaskInfos.end())
        TaskInfos[G] = HCTaskInfo();
      TaskInfos[G].IsCont = true;
    }
  }

  // Assign each continuation type a unique 8-bit tag. Walk the program in
  // IRProgram order (source order, independent of TaskInfos' own order) so tag
  // values are stable across runs. Tag 0 is reserved for "untagged"; the high 8
  // bits of a continuation's closure address carry this tag so that a task with
  // multiple send destinations can route by it.
  unsigned NextTag = 1;
  for (auto &FPtr : P) {
    auto It = TaskInfos.find(FPtr.get());
    if (It == TaskInfos.end() || !It->second.IsCont)
      continue;
    if (NextTag > 0xFF) {
      PANIC("more than 255 continuation types; 8-bit continuation tag exhausted");
    }
    It->second.Tag = (uint8_t)NextTag++;
  }

  // If no explicit continuation exists at all, seed root tasks with a synthetic
  // base continuation so the scheduler has a termination target.
  bool HasExplicitContinuation = false;
  for (auto &[_, Info] : TaskInfos)
    HasExplicitContinuation |= Info.IsCont;
  // if (!HasExplicitContinuation) {
  //   auto *BaseCont = ensureBaseContinuation();
  //   for (auto &[_, Info] : TaskInfos) {
  //     if (Info.IsRoot)
  //       Info.SendArgList.insert(BaseCont);
  //   }
  // }

  // Compute task struct sizes, padding, and return types.
  for (auto &[T, Info] : TaskInfos) {
    Info.TaskSize = 8 + (Info.IsCont ? 4 : 0);
    for (auto &Var : T->Vars) {
      if (Var.DeclLoc == IRVarDecl::ARG) {
        if (Var.Type->isLValueReferenceType()) {
          PANIC("LValueReference type '%s' is not supported as a task spawn or "
                "spawn_next argument (function '%s')",
                Var.Type.getAsString().c_str(), T->getName().c_str());
        }
        auto *HCT = clangTypeToHardCilk(Var.Type);
        Info.TaskSize += hardCilkTypeSize(HCT);
        delete HCT;
      }
    }
    uint32_t TaskWidthBytes =
        (uint32_t)llvm::NextPowerOf2(Info.TaskSize - 1);
    Info.TaskPadding = TaskWidthBytes - Info.TaskSize;
    if (!Info.RetTy)
      Info.RetTy = std::unique_ptr<HardCilkType>(
          clangTypeToHardCilk(T->getReturnType()));
  }

  analyzeSendArguments(P, TaskInfos);

  // Root task continuations signal themselves: the scheduler seeds execution by
  // creating a base closure of the continuation type, so when that continuation
  // fires it decrements another instance of its own type.
  for (auto &FPtr : P) {
    IRFunction *F = FPtr.get();
    auto FIt = TaskInfos.find(F);
    if (FIt == TaskInfos.end() || !FIt->second.IsRoot)
      continue;
    for (auto *ContFn : F->Info.SpawnNextList) {
      auto ContIt = TaskInfos.find(ContFn);
      if (ContIt == TaskInfos.end())
        continue;
      ContIt->second.SendArgList.insert(ContFn);
    }
  }

  analyzeArgOutWriteBuffers(TaskInfos);
  analyzeHasAXI(P, TaskInfos);

  // Terminal continuation tasks with an empty SendArgList are program exit
  // points. Insert the synthetic base continuation as a placeholder to trigger
  // argOut.write() emission in the printer.
  // for (auto &[F, Info] : TaskInfos) {
  //   if (!Info.IsCont || !Info.SendArgList.empty())
  //     continue;
  //   if (!F->Info.SpawnList.empty() || !F->Info.SpawnNextList.empty())
  //     continue;
  //   Info.SendArgList.insert(ensureBaseContinuation());
  // }

  return Result;
}
