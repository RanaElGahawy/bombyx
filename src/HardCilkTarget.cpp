#include "HardCilkTarget.hpp"
#include "IR.hpp"
#include "clang/AST/Type.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <llvm/Support/JSON.h>

#include "OpenCilk2IR.hpp"

#define ALIGN(x, A) ((x + A - 1) & -(A))
#define PADDING(x, A) (ALIGN(x, A) - x)

template <typename T> static T *hctGetIf(HardCilkType *Ty) {
  return Ty ? std::get_if<T>(&Ty->V) : nullptr;
}

template <typename T> static const T *hctGetIf(const HardCilkType *Ty) {
  return Ty ? std::get_if<T>(&Ty->V) : nullptr;
}

bool typeIsVoid(const HardCilkType &Ty) {
  const HardCilkBaseType *BTy = std::get_if<HardCilkBaseType>(&Ty.V);
  return BTy && (*BTy == TY_VOID);
}

HardCilkRecordType clangRecordTypeToHardCilk(const RecordDecl *RD) {
  // loop through the fields of the record type
  std::vector<HardCilkRecordField> fields;
  for (auto field : RD->fields()) {
    std::unique_ptr<HardCilkType> HCT(clangTypeToHardCilk(field->getType()));
    fields.push_back(
        HardCilkRecordField{field->getName().str(), std::move(HCT)});
  }
  return HardCilkRecordType{.Name = RD->getName().str(),
                            .Fields = std::move(fields)};
}

static QualType desugar(QualType QT) {
  while (true) {
    const Type *T = QT.getTypePtrOrNull();
    if (!T)
      break;

    if (auto *ET = dyn_cast<ElaboratedType>(T)) {
      QT = ET->getNamedType();
      continue;
    }
    if (auto *TT = dyn_cast<TypedefType>(T)) {
      QT = TT->desugar();
      continue;
    }
    if (auto *AT = dyn_cast<AttributedType>(T)) {
      QT = AT->getEquivalentType();
      continue;
    }
    break;
  }
  return QT;
}

HardCilkType *clangTypeToHardCilk(IRType &Ty) {
  HardCilkType *HCT = new HardCilkType();

  QualType QT = desugar(Ty);

  if (auto *BTy = dyn_cast<BuiltinType>(QT.getTypePtr())) {
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
      *HCT = TY_UINT32;
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
  } else if (auto *AT = dyn_cast<ConstantArrayType>(QT.getTypePtr())) {
    HardCilkArrayType ArrayTy;
    ArrayTy.Count = AT->getSize().getZExtValue();
    ArrayTy.Elem.reset(clangTypeToHardCilk(AT->getElementType()));
    *HCT = std::move(ArrayTy);
  } else if (auto *RT = QT->getAs<RecordType>()) {
    *HCT = clangRecordTypeToHardCilk(RT->getAsRecordDecl());
  } else if (QT->isLValueReferenceType()) {
    // Strip the reference and convert the pointee type. References are
    // preserved as `&` in inlinable function signatures; tasks must reject
    // them explicitly before calling here.
    QualType PointeeTy = QT->getAs<ReferenceType>()->getPointeeType();
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
  case TY_UINT8:
    return 1;
  case TY_UINT16:
    return 2;
  case TY_UINT32:
    return 4;
  case TY_UINT64:
    return 8;
  case TY_ADDR:
    return 8;
  case TY_FLOAT32:
    return 4;
  case TY_FLOAT64:
    return 8;
  default:
    return -1;
  }
  return -1;
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

const char *printHardCilkType(HardCilkBaseType Ty) {
  switch (Ty) {
  case TY_UINT8:
    return "uint8_t";
  case TY_UINT16:
    return "uint16_t";
  case TY_UINT32:
    return "uint32_t";
  case TY_UINT64:
    return "uint64_t";
  case TY_ADDR:
    return "addr_t";
  case TY_VOID:
    return "void";
  case TY_FLOAT32:
    return "float";
  case TY_FLOAT64:
    return "double";

  default:
    return nullptr;
  }
}

llvm::raw_ostream &printHardCilkType(llvm::raw_ostream &Out, HardCilkType *Ty,
                                     bool Short = false) {
  if (auto *BTy = hctGetIf<HardCilkBaseType>(Ty)) {
    Out << printHardCilkType(*BTy);
  } else if (auto *RTy = hctGetIf<HardCilkRecordType>(Ty)) {
    if (!Short) {
      Out << "struct ";
    }
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
    if (AddRef) {
      PANIC("Cannot print reference declarator for HardCilk array type");
    }
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

static uint32_t hardCilkTypeBitWidth(HardCilkBaseType Ty) {
  return hardCilkTypeSize(Ty) * 8;
}

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
  QualType StoreTy;
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

private:
  void handleScope(ScopeEvent) override {}

  void visitBlock(IRBasicBlock *B) override {
    for (auto &S : *B)
      Stmts.push_back(S.get());
    if (B->Term)
      Stmts.push_back(B->Term);
  }
};
} // namespace

void HardCilkTarget::analyzeSendArguments() {
  bool SomethingHappened = true;

  while (SomethingHappened) {
    SomethingHappened = false;

    for (auto &FPtr : P) {
      IRFunction *F = FPtr.get();
      auto FInfoIt = TaskInfos.find(F);
      std::set<IRFunction *> ParentSendArgList;
      if (FInfoIt != TaskInfos.end()) {
        ParentSendArgList.insert(FInfoIt->second.SendArgList.begin(),
                                 FInfoIt->second.SendArgList.end());
      }
      for (auto &B : *F) {
        for (auto &S : *B) {
          // 1. A function F spawned with the continuation pointing at the
          // closure of a function G has G in its send argument list.
          if (auto *ES = dyn_cast<ESpawnIRStmt>(S.get())) {
            if (!ES->SN) {
              if (TaskInfos.find(ES->Fn) == TaskInfos.end())
                continue;
              auto &ESFInfo = TaskInfos[ES->Fn];
              auto SizeI = ESFInfo.SendArgList.size();
              ESFInfo.SendArgList.insert(ParentSendArgList.begin(),
                                         ParentSendArgList.end());
              // Only propagate SpawnNextList when F is itself a HardCilk task.
              // Entry-point functions (not in TaskInfos) have no parent
              // continuation in the HardCilk model and must not contribute here.
              if (FInfoIt != TaskInfos.end())
                ESFInfo.SendArgList.insert(F->Info.SpawnNextList.begin(),
                                           F->Info.SpawnNextList.end());
              SomethingHappened =
                  SomethingHappened || (SizeI != (ESFInfo.SendArgList.size()));
              continue;
            }
            // Only register the SN continuation when F is a HardCilk task.
            // Entry-point functions (not in TaskInfos) spawn with their own
            // continuation but that continuation is subsumed by the reentry
            // path that IS a task, so registering it here causes duplicates.
            if (FInfoIt == TaskInfos.end())
              continue;
            if (TaskInfos.find(ES->Fn) == TaskInfos.end() ||
                TaskInfos.find(ES->SN->Fn) == TaskInfos.end())
              continue;
            auto &ESFInfo = TaskInfos[ES->Fn];
            auto SizeI = ESFInfo.SendArgList.size();
            ESFInfo.SendArgList.insert(ES->SN->Fn);
            SomethingHappened =
                SomethingHappened || (SizeI != (ESFInfo.SendArgList.size()));
          }
        }
        SpawnNextIRStmt *SNS = nullptr;
        // 2. A continuation inherits all of its root function's send argument
        // destinations.
        if (B->Term && (SNS = dyn_cast<SpawnNextIRStmt>(B->Term))) {
          if (TaskInfos.find(SNS->Fn) == TaskInfos.end())
            continue;
          auto &ContInfo = TaskInfos[SNS->Fn];
          auto SizeI = ContInfo.SendArgList.size();
          ContInfo.SendArgList.insert(ParentSendArgList.begin(),
                                      ParentSendArgList.end());
          SomethingHappened =
              SomethingHappened || (SizeI != (ContInfo.SendArgList.size()));
        }
      }
    }
  }
}

void HardCilkTarget::analyzeArgOutWriteBuffers() {
  for (auto &[F, Info] : TaskInfos) {
    Info.GenerateArgOutWriteBuffer = false;
    Info.BufferedArgumentBits = 0;
    Info.BufferedArgType = TY_VOID;
    Info.BufferedStoreAllowMap.clear();

    if (Info.IsSynthetic || Info.SendArgList.empty() || !typeIsVoid(*Info.RetTy))
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
    Info.BufferedStoreAllowMap[BufferedStores.front()] = 1;
  }
}

IRFunction *HardCilkTarget::ensureBaseContinuation() {
  if (SyntheticBaseContinuation) {
    return SyntheticBaseContinuation.get();
  }

  SyntheticBaseContinuation = std::make_unique<IRFunction>(
      0, "base_continuation", QualType(), &P);
  auto *BaseCont = SyntheticBaseContinuation.get();
  auto &BaseContInfo = TaskInfos[BaseCont];
  BaseContInfo.IsCont = true;
  BaseContInfo.IsSynthetic = true;
  BaseContInfo.RetTy = std::make_unique<HardCilkType>(TY_VOID);
  return BaseCont;
}

HardCilkTarget::HardCilkTarget(IRProgram &P, const std::string &AppName)
    : P(P), AppName(AppName) {

  // Pass 1: register every espawn target as a task. This determines which
  // functions are reachable as HardCilk tasks (i.e., they appear in some
  // function's SpawnList).
  for (auto &F : P) {
    for (auto &G : F->Info.SpawnList) {
      if (TaskInfos.find(G) == TaskInfos.end()) {
        TaskInfos[G] = HCTaskInfo();
      }
      TaskInfos[G].IsRoot |= !F->Info.IsTask;
    }
  }

  // Pass 2: register spawnNext continuations ONLY for functions that are
  // themselves in TaskInfos (i.e., reachable as tasks). Entry-point functions
  // that are never espawned (e.g. the original user function before FlattenIR
  // introduced a reentry) are excluded here. Their continuations are
  // structurally identical to the reentry's continuations and would otherwise
  // create dead duplicate tasks in the output.
  for (auto &F : P) {
    if (TaskInfos.find(F.get()) == TaskInfos.end())
      continue;
    for (auto &G : F->Info.SpawnNextList) {
      if (TaskInfos.find(G) == TaskInfos.end()) {
        TaskInfos[G] = HCTaskInfo();
      }
      TaskInfos[G].IsCont = true;
    }
  }
  bool HasExplicitContinuation = false;
  for (auto &[_, Info] : TaskInfos) {
    HasExplicitContinuation |= Info.IsCont;
  }
  if (!HasExplicitContinuation) {
    auto *BaseCont = ensureBaseContinuation();
    for (auto &[_, Info] : TaskInfos) {
      if (Info.IsRoot) {
        Info.SendArgList.insert(BaseCont);
      }
    }
  }
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
    Info.TaskPadding = PADDING(Info.TaskSize, 32);
    if (!Info.RetTy) {
      Info.RetTy = std::unique_ptr<HardCilkType>(
          clangTypeToHardCilk(T->getReturnType()));
    }
  }
  analyzeSendArguments();
  analyzeArgOutWriteBuffers();

  // Terminal IsCont tasks (no spawns, no further continuations) with an empty
  // SendArgList are the program's exit points. They must emit
  // argOut.write(args._cont) to signal completion back to the caller, so
  // populate their SendArgList with the synthetic base continuation as a
  // placeholder that triggers argOut generation without emitting extra code.
  for (auto &[F, Info] : TaskInfos) {
    if (!Info.IsCont || !Info.SendArgList.empty())
      continue;
    if (!F->Info.SpawnList.empty() || !F->Info.SpawnNextList.empty())
      continue;
    Info.SendArgList.insert(ensureBaseContinuation());
  }
}

llvm::json::Object getSchedulerSide(HCTaskInfo &TaskInfo) {
  llvm::json::Object obj;
  obj["sideType"] = "scheduler";
  obj["numVirtualServers"] = 1;
  obj["capacityVirtualQueue"] = 4096;
  obj["capacityPhysicalQueue"] = 64;
  int64_t totalSize = (TaskInfo.TaskSize + TaskInfo.TaskPadding) * 8;
  obj["portWidth"] = totalSize;
  return obj;
}

llvm::json::Object getArgumentNotifierSide() {
  llvm::json::Object obj;
  obj["sideType"] = "argumentNotifier";
  obj["numVirtualServers"] = 1;
  obj["capacityVirtualQueue"] = 128;
  obj["capacityPhysicalQueue"] = 32;
  obj["portWidth"] = 64;
  return obj;
}

llvm::json::Object getAllocatorSide() {
  llvm::json::Object obj;
  obj["sideType"] = "allocator";
  obj["numVirtualServers"] = 1;
  obj["capacityVirtualQueue"] = 4096;
  obj["capacityPhysicalQueue"] = 32;
  obj["portWidth"] = 64;
  return obj;
}

llvm::json::Object printTaskDescriptor(IRFunction *Task, HCTaskInfo &TaskInfo) {
  llvm::json::Object obj;
  obj["name"] = Task->getName();
  obj["peHDLPath"] = "?";
  obj["isRoot"] = TaskInfo.IsRoot;
  obj["isCont"] = TaskInfo.IsCont;
  obj["dynamicMemAlloc"] = false;
  int64_t closureSize =
      (TaskInfo.TaskPadding + TaskInfo.TaskSize * 8); // closure size in bits
  obj["widthTask"] = closureSize;
  obj["widthMalloc"] = 0;
  obj["variableSpawn"] = false;
  std::vector<llvm::json::Value> sidesConfigs{getSchedulerSide(TaskInfo)};
  if (TaskInfo.IsCont) {
    sidesConfigs.push_back(getArgumentNotifierSide());
    sidesConfigs.push_back(getAllocatorSide());
  }
  if (TaskInfo.GenerateArgOutWriteBuffer) {
    obj["generateArgOutWriteBuffer"] = true;
    std::vector<llvm::json::Value> ArgumentSizeList;
    ArgumentSizeList.push_back(
        llvm::json::Value(static_cast<int64_t>(TaskInfo.BufferedArgumentBits)));
    obj["argumentSizeList"] = std::move(ArgumentSizeList);
  }
  obj["sidesConfigs"] = sidesConfigs;
  return obj;
}

void HardCilkTarget::PrintDescJson(llvm::raw_ostream &Out) {
  llvm::json::Object obj;
  obj["name"] = AppName;
  std::vector<llvm::json::Value> taskDescriptors;
  llvm::json::Object spawnList;
  llvm::json::Object spawnNextList;
  llvm::json::Object sendArgumentList;
  llvm::json::Object mallocList;
  for (auto &[F, Info] : TaskInfos) {
    taskDescriptors.push_back(printTaskDescriptor(F, Info));
    std::vector<llvm::json::Value> spawnListF;
    for (auto G : F->Info.SpawnList) {
      spawnListF.push_back(llvm::json::Value(G->getName()));
    }
    std::vector<llvm::json::Value> spawnNextListF;
    for (auto G : F->Info.SpawnNextList) {
      spawnNextListF.push_back(llvm::json::Value(G->getName()));
    }
    std::vector<llvm::json::Value> sendArgumentListF;
    for (auto G : Info.SendArgList) {
      sendArgumentListF.push_back(llvm::json::Value(G->getName()));
    }
    spawnList[F->getName()] = spawnListF;
    spawnNextList[F->getName()] = spawnNextListF;
    sendArgumentList[F->getName()] = sendArgumentListF;
  }
  obj["taskDescriptors"] = taskDescriptors;
  obj["spawnList"] = llvm::json::Value(std::move(spawnList));
  obj["spawnNextList"] = llvm::json::Value(std::move(spawnNextList));
  obj["sendArgumentList"] = llvm::json::Value(std::move(sendArgumentList));
  obj["mallocList"] = llvm::json::Value(std::move(mallocList));
  obj["widthAddress"] = 64;
  obj["widthContCounter"] = 32;
  obj["memorySizeSim"] = 16;
  obj["targetFrequency"] = 300;
  obj["fpgaModel"] = "ALVEO_U55C";
  llvm::json::Value objV(std::move(obj));
  Out << llvm::formatv("{0:2}", objV);
}

const char *DESCRIPTOR_TEMPLATE = R"(#pragma once
#include <cstdint>
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

)";

#define TAB "  "

// Forward declarations for expression helpers defined later in this file.
void handleArrow(AccessIRExpr *AE, IRPrintContext *C, llvm::raw_ostream &Out);
void handleArray(IndexIRExpr *IE, IRPrintContext *C, llvm::raw_ostream &Out);
void handleDeref(DRefIRExpr *DE, IRPrintContext *C, llvm::raw_ostream &Out);
void handleRef(RefIRExpr *RE, IRPrintContext *C, llvm::raw_ostream &Out);
static bool emitMemStore(llvm::raw_ostream &Out, IRPrintContext &C,
                         StoreIRStmt *SS);
static const FieldDecl *getAccessFieldDecl(AccessIRExpr *AE);
static std::string getAccessStructName(AccessIRExpr *AE);

// Walk every CallIRExpr in F's body and collect the resolved IRFunction* callees.
static void collectDirectCallees(IRFunction *F,
                                 std::set<IRFunction *> &Callees) {
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
    // ESpawnIRStmt args, ClosureDeclIRStmt, SyncIRStmt,
    // BreakIRStmt, ScopeAnnotIRStmt — no plain calls to collect
  };
  for (auto &B : *F) {
    for (auto &S : *B)
      visitStmt(S.get());
    if (B->Term)
      visitStmt(B->Term);
  }
}

// Compute the set of all IRFunctions that need a void *mem parameter:
// a function needs mem if it has any addr_t argument, OR if it calls
// (transitively) any function that needs mem.
static std::set<IRFunction *> computeFuncsNeedingMem(IRProgram &P) {
  std::set<IRFunction *> Result;
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
      std::set<IRFunction *> Callees;
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

// Printer for inlinable helper functions — no HLS stream interface, no task
// closure. Parameters are accessed by name directly. Return is printed as-is.
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

static void PrintInlinableFunction(llvm::raw_ostream &Out, clang::ASTContext &C,
                                   IRFunction *Fn,
                                   const std::set<IRFunction *> &FuncsNeedingMem) {
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
      .IdentCB =
          [](llvm::raw_ostream &Out, IRVarRef VR) {
            Out << GetSym(VR->Name);
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
                Out << std::get<ASTVarRef>(CE->Fn)->getName();
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
            } else {
              E->print(Out, *C);
            }
          }};

  HardCilkInlinablePrinter Printer(Out, IRC);
  Printer.traverse(*Fn);
  Out << "}\n\n";
}

static bool taskHasArgDataOut(const HCTaskInfo &Info) {
  return !typeIsVoid(*Info.RetTy) || Info.GenerateArgOutWriteBuffer;
}

static HardCilkType *getTaskArgDataType(const HCTaskInfo &Info,
                                        HardCilkType &ScratchTy) {
  if (!typeIsVoid(*Info.RetTy)) {
    return Info.RetTy.get();
  }
  ScratchTy = Info.BufferedArgType;
  return &ScratchTy;
}

class HardCilkPrinter : public ScopedIRTraverser {
private:
  llvm::raw_ostream &Out;
  IRPrintContext &C;
  TaskInfosTy const &TaskInfos;
  int SpawnCtr = 0;
  int ArgDataCtr = 0;
  int IndentLvl = 1;

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

    Indent() << SpawnNextFnName << "_spawn_next " << SpawnNextName << ";\n";
    Indent() << SpawnNextName << ".addr = SN_" << SpawnNextFnName << "c_k;\n";
    Indent() << SpawnNextName << ".data = SN_" << SpawnNextFnName << "c;\n";
    auto SnInfoIt = TaskInfos.find(S->Fn);
    assert(SnInfoIt != TaskInfos.end());
    auto &SnInfo = SnInfoIt->second;
    size_t SnTaskSize = llvm::Log2_32_Ceil(SnInfo.TaskSize);
    Indent() << SpawnNextName << ".size = " << SnTaskSize << ";\n";
    // TODO: is that what allow should be?
    Indent() << SpawnNextName << ".allow = SN_" << SpawnNextFnName
             << "c_cnt;\n";
    Indent() << "spawnNext.write(" << SpawnNextName << ");\n\n";
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

    // we expect the functions to be in argument first order
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
      Indent() << "taskGlobalOut.write(" << SpawnFnArgsName << ");\n\n";
    }
  }

  void handleSendArg(ReturnIRStmt *RS, IRFunction *F) {
    if (F->isVoid()) {
      return;
    }
    Indent() << "argOut.write(args._cont);\n";
    HardCilkType *RetType = clangTypeToHardCilk(F->getReturnType());
    printHardCilkType(Indent(), RetType, true)
        << "_arg_out a" << ArgDataCtr << ";\n";
    Indent() << "a" << ArgDataCtr << ".addr = args._cont;\n";
    Indent() << "a" << ArgDataCtr << ".data = ";
    C.ExprCB(&C, Out, RS->RetVal.get());
    Out << ";\n";
    size_t RetTypeSz = llvm::Log2_32_Ceil(hardCilkTypeSize(RetType));
    Indent() << "a" << ArgDataCtr << ".size = " << RetTypeSz << "; "
             << "// TODO calculation could be wrong fix manually for now\n";
    Indent() << "a" << ArgDataCtr << ".allow = 1;\n";
    Indent() << "argDataOut.write(a" << ArgDataCtr << ");\n";
    ArgDataCtr++;
    delete RetType;
  }

  bool emitBufferedStore(StoreIRStmt *SS, IRFunction *F) {
    auto TaskInfoIt = TaskInfos.find(F);
    assert(TaskInfoIt != TaskInfos.end());
    auto &Info = TaskInfoIt->second;
    auto AllowIt = Info.BufferedStoreAllowMap.find(SS);
    if (AllowIt == Info.BufferedStoreAllowMap.end()) {
      return false;
    }

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
    Indent() << "argDataOut.write(a" << ArgDataCtr << ");\n";
    ArgDataCtr++;
    return true;
  }

  void visitStmt(IRStmt *S, IRBasicBlock *B) {
    auto *F = B->getParent();
    if (S->Silent)
      return;

    if (auto *ES = dyn_cast<ESpawnIRStmt>(S)) {
      handleSpawn(ES, F);
      SpawnCtr++;
    } else if (auto *SNS = dyn_cast<SpawnNextIRStmt>(S)) {
      handleSpawnNext(SNS, F);
    } else if (auto *CDS = dyn_cast<ClosureDeclIRStmt>(S)) {
      handleSpawnNextDecl(CDS, F);
    } else if (auto *RS = dyn_cast<ReturnIRStmt>(S)) {
      handleSendArg(RS, F);
    } else if (auto *SS = dyn_cast<StoreIRStmt>(S)) {
      if (!emitBufferedStore(SS, F)) {
        Indent();
        if (!emitMemStore(Out, C, SS))
          S->print(Out, C);
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
  HardCilkPrinter(llvm::raw_ostream &Out, IRPrintContext &C,
                  TaskInfosTy const &TaskInfos)
      : Out(Out), C(C), TaskInfos(TaskInfos) {}
};

void PrintHardCilkTask(llvm::raw_ostream &Out, clang::ASTContext &C,
                       HardCilkPrinter &Printer, IRFunction *Task,
                       HCTaskInfo &Info,
                       const std::set<IRFunction *> &FuncsNeedingMem) {
  std::vector<std::pair<std::string, std::string>> intfs;
  Out << "void " << Task->getName() << " (\n";
  intfs.push_back(std::make_pair("taskIn", Task->getName() + "_task"));
  bool HasMem = FuncsNeedingMem.count(Task) > 0;
  if (HasMem) {
    Out << "  void *mem,\n";
  }
  bool SpawnsItself = false;
  assert(Task->Info.SpawnList.size() <= 2);
  for (IRFunction *SpawnTask : Task->Info.SpawnList) {
    if (SpawnTask == Task) {
      intfs.push_back(std::make_pair("taskOut", Task->getName() + "_task"));
    } else {
      intfs.push_back(
          std::make_pair("taskGlobalOut", SpawnTask->getName() + "_task"));
    }
  }
  if (Info.SendArgList.size() > 0) {
    if (Info.SendArgList.size() > 1) {
      PANIC("UNSUPPORTED: more than one send argmuent destination");
    }
    bool NeedsVoidSend = Task->Info.SpawnNextList.empty();
    if (!Task->isVoid() || NeedsVoidSend) {
      intfs.push_back(std::make_pair("argOut", "uint64_t"));
    }
    if (!typeIsVoid(*Info.RetTy) ||
        (Info.GenerateArgOutWriteBuffer && NeedsVoidSend)) {
      std::string ArgDataOutTy;
      llvm::raw_string_ostream ArgDataOutTyS(ArgDataOutTy);
      HardCilkType ScratchTy;
      printHardCilkType(ArgDataOutTyS, getTaskArgDataType(Info, ScratchTy),
                        true);
      intfs.push_back(std::make_pair("argDataOut", ArgDataOutTy + "_arg_out"));
    }
  }
  if (Task->Info.SpawnNextList.size() > 0) {
    if (Task->Info.SpawnNextList.size() > 1) {
      PANIC("UNSUPPORTED: more than one spawn next in a function");
    }
    auto &SNDest = *Task->Info.SpawnNextList.begin();
    intfs.push_back(std::make_pair("closureIn", "uint64_t"));
    intfs.push_back(
        std::make_pair("spawnNext", SNDest->getName() + "_spawn_next"));
  }

  bool first = true;
  for (auto &[intfName, intfTy] : intfs) {
    if (!first) {
      Out << ",\n";
    }
    Out << "  hls::stream<" << intfTy << "> &" << intfName;
    first = false;
  }
  Out << "\n) {\n\n";

  for (auto &[intfName, _] : intfs) {
    Out << "#pragma HLS INTERFACE mode = axis port = " << intfName << "\n";
  }
  if (HasMem) {
    Out << "#pragma HLS INTERFACE mode = m_axi port = mem\n";
  }
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
  Printer.traverse(*Task);
  if (Info.SendArgList.size() > 0 && Task->isVoid() &&
      Task->Info.SpawnNextList.empty()) {
    Out << TAB << "argOut.write(args._cont);\n";
  }
  Out << "}\n\n";
}

static const FieldDecl *getAccessFieldDecl(AccessIRExpr *AE) {
  IRVarRef SR = AE->getStructVarRef();
  if (!SR)
    return nullptr;

  QualType BaseTy = AE->Arrow ? SR->Type->getPointeeType() : SR->Type;
  BaseTy = desugar(BaseTy);
  auto *RT = BaseTy->getAs<RecordType>();
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
  QualType BaseTy = AE->Arrow ? SR->Type->getPointeeType() : SR->Type;
  BaseTy = desugar(BaseTy);
  if (auto *RT = BaseTy->getAs<RecordType>()) {
    return RT->getDecl()->getName().str();
  }

  std::string StructName = BaseTy.getUnqualifiedType().getAsString();
  if (StructName.rfind("struct ", 0) == 0) {
    StructName.erase(0, 7);
  }
  if (StructName.empty()) {
    PANIC("Could not resolve accessed struct type for '%s'",
          AE->Field.c_str());
  }
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

// Emit MEM_ARR_OUT / MEM_OUT for writes through pointer lvalues.
// Returns true if the store was emitted; caller must append ";\n".
static bool emitMemStore(llvm::raw_ostream &Out, IRPrintContext &C,
                         StoreIRStmt *SS) {
  if (auto *IE = dyn_cast<IndexIRExpr>(SS->Dest.get())) {
    if (auto *AE = dyn_cast<AccessIRExpr>(IE->Arr.get())) {
      if (AE->Arrow) {
        if (auto *Field = getAccessFieldDecl(AE);
            Field && Field->getType()->isArrayType()) {
          IRVarRef SR = AE->getStructVarRef();
          assert(SR &&
                 "emitMemStore: non-ident struct base not yet supported");
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

void handleRef(RefIRExpr *RE, IRPrintContext *C, llvm::raw_ostream &Out) {
  // &arr[i] is pointer arithmetic — addr of arr + i*sizeof(elem). No memory read.
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

void HardCilkTarget::PrintHardCilk(llvm::raw_ostream &Out,
                                   clang::ASTContext &C) {
  Out << "#include \"hls_stream.h\"\n";
  Out << "#include \"" << AppName << "_defs.h\"\n\n";

  const std::set<IRFunction *> FuncsNeedingMem = computeFuncsNeedingMem(P);

  IRPrintContext IRC = IRPrintContext{
      .ASTCtx = C,
      .NewlineSymbol = "\n",
      .IdentCB =
          [&](llvm::raw_ostream &Out, IRVarRef VR) {
            switch (VR->DeclLoc) {
            case IRVarDecl::ARG: {
              Out << "args." << GetSym(VR->Name);
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
      .ExprCB =
          [&](IRPrintContext *C, llvm::raw_ostream &Out, IRExpr *E) {
            if (auto *CE = dyn_cast<CallIRExpr>(E)) {
              IRFunction *Callee = nullptr;
              if (auto *FP = std::get_if<IRFunction *>(&CE->Fn))
                Callee = *FP;
              if (Callee)
                Out << Callee->getName();
              else
                Out << std::get<ASTVarRef>(CE->Fn)->getName();
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
              if (AE->Arrow) {
                handleArrow(AE, C, Out);
              } else {
                AE->print(Out, *C);
              }
            } else if (auto *DE = dyn_cast<DRefIRExpr>(E)) {
              handleDeref(DE, C, Out);
            } else if (auto *IE = dyn_cast<IndexIRExpr>(E)) {
              handleArray(IE, C, Out);
            } else if (auto *RE = dyn_cast<RefIRExpr>(E)) {
              handleRef(RE, C, Out);
            } else {
              E->print(Out, *C);
            }
          }};

  // Emit inlinable helper functions first so tasks can call them.
  // An inlinable is any IRFunction in P that is not a cilk task/continuation
  // and has no spawn statements of its own.
  for (auto &FPtr : P) {
    IRFunction *F = FPtr.get();
    if (TaskInfos.find(F) != TaskInfos.end())
      continue;
    if (!F->Info.SpawnList.empty() || !F->Info.SpawnNextList.empty())
      continue;
    // Skip dead task/continuation functions that were never registered in
    // TaskInfos (e.g. the duplicate continuation of an entry-point function
    // whose reentry already carries the canonical continuation).
    if (F->Info.IsTask)
      continue;
    PrintInlinableFunction(Out, C, F, FuncsNeedingMem);
  }

  HardCilkPrinter Printer(Out, IRC, TaskInfos);
  for (auto &[T, Info] : TaskInfos) {
    if (Info.IsSynthetic)
      continue;
    PrintHardCilkTask(Out, C, Printer, T, Info, FuncsNeedingMem);
  }
}

void HardCilkTarget::PrintDef(llvm::raw_ostream &Out, IRFunction *Task,
                              HCTaskInfo &Info) {
  Out << "struct __attribute__((packed))" << Task->getName() << "_task {\n";
  Out << "  addr_t _cont;\n";
  if (Info.IsCont) {
    Out << "  uint32_t _counter;\n";
  }
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
  if (Info.TaskPadding > 0) {
    Out << "  uint8_t _padding[" << Info.TaskPadding << "];\n";
  }
  Out << "};\n\n";

  if (Info.IsCont) {
    size_t SnSize = Info.TaskSize + Info.TaskPadding +
                    hardCilkTypeSize(TY_ADDR) + hardCilkTypeSize(TY_UINT32) * 2;
    size_t SnPadding = PADDING(SnSize, 32);
    Out << "struct " << Task->getName() << "_spawn_next {\n";
    Out << "  addr_t addr;\n";
    Out << "  " << Task->getName() << "_task data;\n";
    Out << "  uint32_t size;\n";
    Out << "  uint32_t allow;\n";
    if (SnPadding > 0) {
      Out << "  uint8_t _padding[" << SnPadding << "];\n";
    }
    Out << "};\n\n";
  }

  HardCilkType ScratchTy;
  HardCilkType *ArgDataTy = getTaskArgDataType(Info, ScratchTy);
  bool OkBaseType = true;
  if (taskHasArgDataOut(Info)) {
    if (auto *BTy = hctGetIf<HardCilkBaseType>(ArgDataTy)) {
      OkBaseType = !ArgOutImplList[*BTy] && (*BTy != TY_VOID);
    }
  }
  if (Info.SendArgList.size() != 0 && taskHasArgDataOut(Info) && OkBaseType) {
    printHardCilkType(Out << "struct __attribute__((packed)) ", ArgDataTy,
                      true)
        << "_arg_out {\n";
    Out << "  addr_t addr;\n";
    printHardCilkDecl(Out << "  ", ArgDataTy, "data") << ";\n";
    Out << "  uint32_t size;\n";
    Out << "  uint32_t allow;\n";
    size_t argOutSize = hardCilkTypeSize(ArgDataTy) +
                        hardCilkTypeSize(TY_UINT64) +
                        hardCilkTypeSize(TY_UINT32) * 2;
    size_t argOutPadding = PADDING(argOutSize, 32);
    if (argOutPadding > 0) {
      Out << "  uint8_t _padding[" << argOutPadding << "];\n";
    }
    Out << "};\n\n";
    if (auto BTy = hctGetIf<HardCilkBaseType>(ArgDataTy)) {
      ArgOutImplList[*BTy] = true;
    }
  }
}

void HardCilkTarget::PrintDefs(llvm::raw_ostream &Out) {
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
    PrintDef(Out, T, Info);
  }
}

void HardCilkTarget::PrintDriver(llvm::raw_ostream &Out) {
  IRFunction *DF = nullptr;
  IRFunction *DFC = nullptr;
  for (auto &F : P) {
    if (F->getName() == "bombyx_driver") {
      DF = F.get();
      if (DF->Info.SpawnNextList.size() != 1) {
        PANIC("Driver should have exactly one continuation.");
      }
      DFC = *DF->Info.SpawnNextList.begin();
      if (!DFC->Info.SpawnNextList.empty() || !DFC->Info.SpawnList.empty()) {
        PANIC("Driver continuation should not have continuations, or spawn "
              "anything");
      }
    }
  }
  if (!DF || !DFC) {
    PANIC("To print driver code, please include a function named bombyx_driver "
          "with exactly one cilk_sync.");
  }
  assert(DF->Info.SpawnList.size() == 1 &&
         "unimplemented: handle multiple spawn types in driver");
}
