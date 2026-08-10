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

    // A buffered store leaves through argOut/argDataOut, and those ports exist
    // only on a LEAF SENDER: a task with a non-empty spawn or spawn_next list
    // forwards its argument through the spawned closure and never sends one, so
    // VitisHLSTarget emits no argDataOut port for it (NeedsVoidSend) and
    // HardCilkDescGen leaves it out of the JSON on the same rule. Buffering here
    // would emit `argDataOut...write(...)` against a port that was never
    // declared. Such a store stays a plain m_axi write instead.
    if (!F->Info.SpawnList.empty() || !F->Info.SpawnNextList.empty())
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

  for (auto &[T, Info] : TaskInfos)
    if (T->Info.IsOverlap)
      Info.IsOverlap = true;

  // A continuation is "streaming" — fed in-order by a generated SystemVerilog
  // merge unit, so it skips the closure/allocator/argumentNotifier machinery —
  // only when it is collapsed INTO a wrapper. Belonging to an OVERLAP loop is
  // not enough: when the loop body escapes, the continuation that waits on the
  // real spawned callee stays outside the wrapper and is fed by the ordinary
  // scheduler, so it needs its argumentNotifier and allocator sides like any
  // other continuation. Grouping is decided here rather than from IsOverlap for
  // exactly that reason (the same distinction drives contStateOut vs
  // closureIn/spawnNext in VitisHLSTarget).
  const std::vector<OverlapGroup> OverlapGroups = computeOverlapGroups(TaskInfos);
  for (const OverlapGroup &G : OverlapGroups)
    for (IRFunction *F : G.Internal) {
      auto It = TaskInfos.find(F);
      if (It != TaskInfos.end() && It->second.IsCont)
        It->second.StreamingCont = true;
    }

  // Drop send-argument edges that point into a wrapper's interior. A
  // continuation collapsed into a wrapper is driven solely by that wrapper's
  // merge unit — the wrapper exposes no argIn for it — and by construction none
  // of them waits on a result from outside (the continuation that waits on a
  // real spawned callee is deliberately kept external). analyzeSendArguments
  // over-approximates which continuations a task's _cont may name, so such an
  // edge can survive into a collapsed loop; left in, it becomes a
  // sendArgument edge naming the wrapper, and the HardCilk generator asserts
  // (`ArgumentNotifier.scala: assert(argRouteServersNumber > 0)`) because the
  // wrapper has no argumentNotifier side. Pruning here rather than in the
  // descriptor printer keeps the exit PE's argOut ports in step: they are
  // derived from this same list.
  for (auto &[T, Info] : TaskInfos) {
    for (const OverlapGroup &G : OverlapGroups) {
      // Only edges crossing INTO the wrapper are unroutable. A group member
      // sending to another member is the wrapper's own internal wiring — the
      // memReader's argDataOut feeding the merge unit is exactly that — and
      // dropping it would delete the reader's argOut ports while its body still
      // writes them.
      if (G.Internal.count(T))
        continue;
      // PANIC is an unbraced multi-statement macro; keep it in its own block.
      if (Info.SendArgList.count(G.Entry)) {
        PANIC("task '%s' sends an argument to '%s', the entry of OVERLAP "
              "wrapper '%s'; the wrapper exposes no argIn port for it",
              T->getName().c_str(), G.Entry->getName().c_str(),
              G.WrapperName.c_str());
      }
      for (IRFunction *F : G.Internal)
        if (F != G.Entry)
          Info.SendArgList.erase(F);
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

// ─── OVERLAP wrapper grouping ────────────────────────────────────────────────

// Targets of *dependent* spawns (ESpawnIRStmt with a SpawnNext) in F's body.
// These are the memory-reader leaves the wrapper serves from its u_memreader;
// a plain spawn of an unrelated task is not one of them.
static void collectDependentSpawnTargets(IRFunction *F,
                                         IRFuncSetTy &Out) {
  for (auto &B : *F)
    for (auto &S : *B)
      if (auto *ES = dyn_cast<ESpawnIRStmt>(S.get()))
        if (ES->SN && ES->Fn)
          Out.insert(ES->Fn);
}

std::vector<OverlapGroup> computeOverlapGroups(const TaskInfosTy &TaskInfos) {
  // Bucket the overlap tasks by which loop they came from. Keying on the
  // boolean IsOverlap alone would merge every OVERLAP loop in the program into
  // one wrapper.
  std::map<int, IRFuncSetTy> ById;
  for (auto &[F, Info] : TaskInfos)
    if (Info.IsOverlap)
      ById[F->Info.OverlapId].insert(F);

  // Every set iterated below is IRFuncSetTy, which is already ordered by task
  // NAME (see IRFunctionNameLess), so "last one wins" selections here are
  // stable across runs. byName() is kept as an explicit restatement of that
  // requirement and to normalise the occasional std::vector built from a
  // pointer-ordered source.
  auto byName = [](const IRFuncSetTy &S) {
    std::vector<IRFunction *> V(S.begin(), S.end());
    llvm::sort(V, [](IRFunction *A, IRFunction *B) {
      return A->getName() < B->getName();
    });
    return V;
  };

  std::vector<OverlapGroup> Groups;
  for (auto &[Id, Members] : ById) {
    OverlapGroup G;
    G.Entry = nullptr;
    const std::vector<IRFunction *> MembersV = byName(Members);

    // Memory readers, collected from EVERY member rather than from a single
    // spawn_next chain. A loop nest has one chain per loop level, and the levels
    // are joined by plain tail spawns (`cont1` tail-spawns the inner head), which
    // a SpawnNextList walk cannot follow — it would leave the inner level's
    // readers outside the wrapper and scheduler-visible.
    IRFuncSetTy Readers;
    // Dependent spawns of a *real* (non-reader) task: genuine task parallelism,
    // which the merge unit cannot complete a continuation from.
    std::vector<std::pair<IRFunction *, IRFunction *>> RealDeps; // issuer, target
    for (IRFunction *F : MembersV) {
      IRFuncSetTy Deps;
      collectDependentSpawnTargets(F, Deps);
      for (IRFunction *D : byName(Deps)) {
        if (D->Info.IsMemReader)
          Readers.insert(D);
        else
          RealDeps.push_back({F, D});
      }
    }

    // No decoupled loads means nothing for a wrapper to do.
    if (Readers.empty())
      continue;

    // The loop heads: non-continuation members that own a spawn_next. A single
    // OVERLAP loop has exactly one; a nest has one per level.
    std::vector<IRFunction *> Heads;
    for (IRFunction *F : MembersV) {
      if (!TaskInfos.count(F))
        continue;
      if (!TaskInfos.at(F).IsCont && !F->Info.SpawnNextList.empty())
        Heads.push_back(F);
    }
    if (Heads.empty())
      continue;

    // Members entered from outside the group. Used both to pick Entry and to
    // seed the outermost-head search below.
    IRFuncSetTy ExtEntered;
    for (auto &[F, Info] : TaskInfos) {
      if (Members.count(F) || Readers.count(F))
        continue;
      for (auto *S : F->Info.SpawnList)
        if (Members.count(S))
          ExtEntered.insert(S);
      for (auto *S : F->Info.SpawnNextList)
        if (Members.count(S))
          ExtEntered.insert(S);
    }

    // G.Reentry is the OUTERMOST head: the first head reached by a breadth-first
    // walk from where the group is entered. With one head this is that head; with
    // a nest it is the outer loop's, never the inner loop's.
    {
      std::vector<IRFunction *> Seeds;
      for (IRFunction *F : MembersV)
        if (TaskInfos.count(F) && TaskInfos.at(F).IsRoot)
          Seeds.push_back(F);
      for (IRFunction *F : byName(ExtEntered))
        Seeds.push_back(F);
      if (Seeds.empty())
        Seeds = Heads;

      IRFuncSetTy Seen(Seeds.begin(), Seeds.end());
      std::deque<IRFunction *> Q(Seeds.begin(), Seeds.end());
      auto isHead = [&](IRFunction *F) {
        return llvm::is_contained(Heads, F);
      };
      while (!Q.empty()) {
        IRFunction *F = Q.front();
        Q.pop_front();
        if (isHead(F)) {
          G.Reentry = F;
          break;
        }
        IRFuncSetTy Next(F->Info.SpawnList.begin(),
                                   F->Info.SpawnList.end());
        Next.insert(F->Info.SpawnNextList.begin(), F->Info.SpawnNextList.end());
        for (IRFunction *S : byName(Next))
          if (Members.count(S) && Seen.insert(S).second)
            Q.push_back(S);
      }
      if (!G.Reentry)
        G.Reentry = Heads.front();
    }

    // Escape: some member dependently spawns a real task. The wrapper then
    // stands in for the reentry alone. This is only expressible while the group
    // is a single chain — with a nest there is no single "prefix of the chain"
    // to keep inside, so reject it explicitly rather than emit wiring that
    // cannot work.
    if (!RealDeps.empty()) {
      G.Escapes = true;
      G.EscapeIssuer = RealDeps.front().first;
      if (!G.EscapeIssuer->Info.SpawnNextList.empty())
        G.EscapeCont = *G.EscapeIssuer->Info.SpawnNextList.begin();
      if (Heads.size() > 1) {
        PANIC("#pragma BOMBYX OVERLAP: loop nest '%s' both contains a nested "
              "loop and dependently spawns the real task '%s'; an escaping "
              "nested OVERLAP loop is not supported",
              G.Reentry->getName().c_str(),
              RealDeps.front().second->getName().c_str());
      }
    }

    if (!G.Escapes) {
      // The whole loop nest is internal: the initializer, exit and every
      // continuation at every level collapse into one meta-task.
      G.Internal = Members;
      for (IRFunction *L : Readers)
        G.Internal.insert(L);
      for (IRFunction *F : MembersV)
        if (TaskInfos.count(F) && TaskInfos.at(F).IsRoot) {
          G.Entry = F;
          break;
        }
      // Entry fallback: a loop invoked from another task (not a program root) is
      // entered via whichever of its tasks an outside task spawns. There must be
      // exactly one — the wrapper has a single taskIn.
      if (!G.Entry) {
        const std::vector<IRFunction *> Ext = byName(ExtEntered);
        // NB: PANIC is not a do/while macro — it MUST be braced, or its argument
      // evaluation and its exit() escape the guard.
        if (Ext.size() > 1) {
          PANIC("#pragma BOMBYX OVERLAP: wrapper for '%s' has %zu external entry "
                "points (%s, %s, ...); a wrapper has a single taskIn",
                G.Reentry->getName().c_str(), Ext.size(),
                Ext[0]->getName().c_str(), Ext[1]->getName().c_str());
        }
        if (!Ext.empty())
          G.Entry = Ext.front();
      }
    } else {
      // The loop body escapes. The wrapper stands in for the reentry alone:
      // it is entered with the reentry's closure, both from the loop
      // initializer and from the external continuation that closes the loop.
      // Absorbing the initializer is not possible here — the wrapper has a
      // single taskIn, and it must carry the reentry's closure so the external
      // loop-back edge can land on it.
      //
      // Only the prefix of the spawn_next chain up to and including the escape
      // issuer stays inside; the issuer's real spawn target and the continuation
      // waiting on it are external. The chain walk is well defined here because
      // an escaping group is guaranteed single-level (see the PANIC above).
      IRFuncSetTy Chain, ChainReaders;
      IRFunction *Issuer = G.Reentry;
      IRFuncSetTy Seen;
      while (Issuer && Seen.insert(Issuer).second) {
        Chain.insert(Issuer);
        IRFuncSetTy Deps;
        collectDependentSpawnTargets(Issuer, Deps);
        bool Real = false;
        for (IRFunction *D : byName(Deps)) {
          if (D->Info.IsMemReader)
            ChainReaders.insert(D);
          else
            Real = true;
        }
        if (Real || Issuer->Info.SpawnNextList.empty())
          break;
        Issuer = *Issuer->Info.SpawnNextList.begin();
      }
      G.Internal = Chain;
      for (IRFunction *L : ChainReaders)
        G.Internal.insert(L);
      G.Entry = G.Reentry;
    }

    if (G.Internal.empty() || !G.Entry)
      continue;
    // Name per loop, not per app, so two wrappers can coexist.
    G.WrapperName = G.Entry->getName() + "_overlap_wrapper";
    Groups.push_back(std::move(G));
  }
  return Groups;
}

const OverlapGroup *findOverlapGroup(const std::vector<OverlapGroup> &Groups,
                                     IRFunction *F) {
  for (const OverlapGroup &G : Groups)
    if (G.Internal.count(F))
      return &G;
  return nullptr;
}

// ─── Cacheable memory readers ────────────────────────────────────────────────
//
// Deciding whether an `#pragma HLS cache` on a reader's m_axi port is safe is a
// question about the whole design, not about that PE: the reader only reads, so
// the only way a cached line can go stale is another PE writing the same
// buffer. The analysis is therefore a whole-program one, and it is built to err
// towards "not cacheable" — every construct it does not understand poisons the
// pointer rather than being ignored.

namespace {

// Union-find over pointer variables, joining values that may be the same
// buffer: spawn argument passing (caller var -> callee ARG) and pointer copies.
struct PtrClasses {
  IRVarMapTy<IRVarRef> Parent;

  IRVarRef find(IRVarRef V) {
    auto It = Parent.find(V);
    if (It == Parent.end())
      return Parent[V] = V;
    if (It->second == V)
      return V;
    return Parent[V] = find(It->second);
  }
  void join(IRVarRef A, IRVarRef B) {
    if (!A || !B)
      return;
    A = find(A);
    B = find(B);
    if (A != B)
      Parent[A] = B;
  }
};

// The variable a store writes through, or null when the destination is not a
// simple `p[i]` / `*p` / `p->f` rooted at a named pointer.
IRVarRef storeRootVar(const IRLvalExpr *Dest) {
  const IRExpr *E = Dest;
  while (true) {
    if (auto *IE = dyn_cast<IndexIRExpr>(E)) {
      E = IE->Arr.get();
      continue;
    }
    if (auto *DE = dyn_cast<DRefIRExpr>(E)) {
      E = DE->Expr.get();
      continue;
    }
    if (auto *AE = dyn_cast<AccessIRExpr>(E)) {
      E = AE->Base.get();
      continue;
    }
    if (auto *CE = dyn_cast<CastIRExpr>(E)) {
      E = CE->E.get();
      continue;
    }
    break;
  }
  if (auto *ID = dyn_cast<IdentIRExpr>(E))
    return ID->Ident;
  return nullptr;
}

// A pointer whose value came out of memory (`(uint32_t *)pGraph[2*v]`) has no
// named buffer behind it, so it cannot be connected to the pointers a store
// writes through. Such values are safe only while nothing in the design writes
// through a pointer of unknown provenance either.
bool isLoadedPointer(const IRExpr *Src) {
  const IRExpr *E = Src;
  while (auto *CE = dyn_cast<CastIRExpr>(E))
    E = CE->E.get();
  return isa<IndexIRExpr>(E) || isa<DRefIRExpr>(E) || isa<ISpawnIRExpr>(E);
}

} // namespace

IRFuncSetTy computeReadOnlyReaders(IRProgram &P) {
  IRFuncSetTy Result;

  PtrClasses Classes;
  IRVarSetTy WrittenRoots;   // written through directly
  IRVarSetTy LoadedPtrs;     // value came out of memory
  bool WritesThroughUnknown = false; // a store through an unprovable pointer

  auto isPtr = [](IRVarRef V) {
    return V && !V->Type.isNull() && V->Type->isPointerType();
  };

  // Join a spawn's actual arguments onto the callee's ARG variables.
  auto joinCallArgs = [&](IRFunction *Callee,
                          const std::vector<std::unique_ptr<IRExpr>> &Args) {
    if (!Callee)
      return;
    std::vector<IRVarRef> Params;
    for (auto &V : Callee->Vars)
      if (V.DeclLoc == IRVarDecl::ARG)
        Params.push_back(&V);
    for (size_t I = 0; I < Args.size() && I < Params.size(); I++) {
      const IRExpr *A = Args[I].get();
      while (auto *CE = dyn_cast<CastIRExpr>(A))
        A = CE->E.get();
      if (auto *ID = dyn_cast<IdentIRExpr>(A))
        Classes.join(ID->Ident, Params[I]);
      else if (isPtr(Params[I]) && isLoadedPointer(Args[I].get()))
        LoadedPtrs.insert(Classes.find(Params[I]));
    }
  };

  auto visitStmt = [&](IRStmt *S) {
    if (auto *SS = dyn_cast<StoreIRStmt>(S)) {
      // A store whose destination is a plain local is not a memory write.
      if (!isa<IdentIRExpr>(SS->Dest.get())) {
        if (IRVarRef Root = storeRootVar(SS->Dest.get()))
          WrittenRoots.insert(Root);
        else
          WritesThroughUnknown = true;
      }
      return;
    }
    if (auto *CS = dyn_cast<CopyIRStmt>(S)) {
      if (!isPtr(CS->Dest))
        return;
      const IRExpr *Src = CS->Src.get();
      while (auto *CE = dyn_cast<CastIRExpr>(Src))
        Src = CE->E.get();
      if (auto *ID = dyn_cast<IdentIRExpr>(Src))
        Classes.join(CS->Dest, ID->Ident);
      else if (isLoadedPointer(CS->Src.get()))
        LoadedPtrs.insert(CS->Dest);
      if (auto *IS = dyn_cast<ISpawnIRExpr>(CS->Src.get()))
        if (auto *FP = std::get_if<IRFunction *>(&IS->Fn))
          joinCallArgs(*FP, IS->Args);
      return;
    }
    if (auto *ES = dyn_cast<ESpawnIRStmt>(S)) {
      joinCallArgs(ES->Fn, ES->Args);
      if (ES->Dest && isPtr(storeRootVar(ES->Dest.get())) &&
          !isa<IdentIRExpr>(ES->Dest.get()))
        WritesThroughUnknown = true;
      return;
    }
    if (auto *EW = dyn_cast<ExprWrapIRStmt>(S)) {
      // A ++/-- through memory is a read-modify-write.
      if (auto *UE = dyn_cast<UnopIRExpr>(EW->Expr.get())) {
        bool IncDec = UE->Op == UnopIRExpr::UNOP_PREINC ||
                      UE->Op == UnopIRExpr::UNOP_POSTINC ||
                      UE->Op == UnopIRExpr::UNOP_PREDEC ||
                      UE->Op == UnopIRExpr::UNOP_POSTDEC;
        if (IncDec && !isa<IdentIRExpr>(UE->Expr.get())) {
          if (auto *LV = dyn_cast<IRLvalExpr>(UE->Expr.get())) {
            if (IRVarRef Root = storeRootVar(LV))
              WrittenRoots.insert(Root);
            else
              WritesThroughUnknown = true;
          } else {
            WritesThroughUnknown = true;
          }
        }
      }
      if (auto *IS = dyn_cast<ISpawnIRExpr>(EW->Expr.get()))
        if (auto *FP = std::get_if<IRFunction *>(&IS->Fn))
          joinCallArgs(*FP, IS->Args);
      return;
    }
  };

  // Two passes: the first builds the value-flow classes, the second resolves
  // written roots against the finished classes.
  for (int Pass = 0; Pass < 2; Pass++)
    for (auto &FPtr : P)
      for (auto &B : *FPtr) {
        for (auto &S : *B)
          visitStmt(S.get());
        if (B->Term)
          visitStmt(B->Term);
      }

  IRVarSetTy UnsafeClasses;
  for (IRVarRef V : WrittenRoots)
    UnsafeClasses.insert(Classes.find(V));
  // A store through a pointer that itself came out of memory means we cannot
  // tell any two loaded pointers apart, so none of them is cacheable.
  for (IRVarRef V : LoadedPtrs)
    if (UnsafeClasses.count(Classes.find(V)))
      WritesThroughUnknown = true;

  for (auto &FPtr : P) {
    IRFunction *F = FPtr.get();
    if (!F->Info.IsMemReader)
      continue;
    // The reader's first ARG is the base pointer it dereferences.
    IRVarRef Base = nullptr;
    for (auto &V : F->Vars)
      if (V.DeclLoc == IRVarDecl::ARG) {
        Base = &V;
        break;
      }
    if (!isPtr(Base))
      continue;
    IRVarRef Cls = Classes.find(Base);
    if (UnsafeClasses.count(Cls))
      continue;
    if (WritesThroughUnknown && LoadedPtrs.count(Cls))
      continue;
    Result.insert(F);
  }
  return Result;
}

IRFuncSetTy computeCacheableReaders(IRProgram &P,
                                    const TaskInfosTy &TaskInfos) {
  (void)TaskInfos;
  // Nothing to decide unless some reader actually shows spatial reuse.
  bool AnyHint = false;
  for (auto &FPtr : P)
    AnyHint |= FPtr->Info.SpatialReuseHint;
  if (!AnyHint)
    return {};

  IRFuncSetTy Result;
  for (IRFunction *F : computeReadOnlyReaders(P))
    if (F->Info.SpatialReuseHint)
      Result.insert(F);
  return Result;
}
