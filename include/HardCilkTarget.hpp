#pragma once

#include "IR.hpp"
#include "clang/AST/ASTContext.h"
#include <cstdint>
#include <unordered_map>

enum HardCilkBaseType {
  TY_UINT8,
  TY_UINT16,
  TY_UINT32,
  TY_UINT64,
  TY_ADDR,
  TY_VOID,
  TY_LAST,
  TY_FLOAT32,
  TY_FLOAT64,
};

struct HardCilkType;

struct HardCilkArrayType {
  std::unique_ptr<HardCilkType> Elem;
  uint64_t Count = 0;
};

struct HardCilkRecordField {
  std::string Name;
  std::unique_ptr<HardCilkType> Type;
};

struct HardCilkRecordType {
  std::string Name;
  std::vector<HardCilkRecordField> Fields;
};

struct HardCilkType {
  using VariantTy =
      std::variant<HardCilkBaseType, HardCilkRecordType, HardCilkArrayType>;

  VariantTy V;

  HardCilkType() = default;
  HardCilkType(HardCilkBaseType Ty) : V(Ty) {}
  HardCilkType(HardCilkRecordType Ty) : V(std::move(Ty)) {}
  HardCilkType(HardCilkArrayType Ty) : V(std::move(Ty)) {}

  HardCilkType &operator=(HardCilkBaseType Ty) {
    V = Ty;
    return *this;
  }

  HardCilkType &operator=(HardCilkRecordType Ty) {
    V = std::move(Ty);
    return *this;
  }

  HardCilkType &operator=(HardCilkArrayType Ty) {
    V = std::move(Ty);
    return *this;
  }
};

struct HCTaskInfo {
  std::set<IRFunction *> SendArgList;
  bool IsRoot = false;
  bool IsCont = false;
  bool IsSynthetic = false;
  bool GenerateArgOutWriteBuffer = false;
  uint32_t BufferedArgumentBits = 0;
  HardCilkBaseType BufferedArgType = TY_VOID;
  size_t TaskSize;
  size_t TaskPadding;
  std::unique_ptr<HardCilkType> RetTy;
  std::unordered_map<const StoreIRStmt *, uint32_t> BufferedStoreAllowMap;
  HCTaskInfo() {}
};

using TaskInfosTy = std::unordered_map<IRFunction *, HCTaskInfo>;

HardCilkType *clangTypeToHardCilk(IRType &Ty);

class HardCilkTarget {
private:
  IRProgram &P;
  const std::string &AppName;
  TaskInfosTy TaskInfos;
  std::unique_ptr<IRFunction> SyntheticBaseContinuation;
  bool ArgOutImplList[TY_LAST] = {false};

  void analyzeSendArguments();
  void analyzeArgOutWriteBuffers();
  IRFunction *ensureBaseContinuation();

  void PrintDef(llvm::raw_ostream &Out, IRFunction *Task, HCTaskInfo &Info);

public:
  HardCilkTarget(IRProgram &P, const std::string &AppName);

  void PrintHardCilk(llvm::raw_ostream &out, clang::ASTContext &C);
  void PrintDescJson(llvm::raw_ostream &out);
  void PrintDriver(llvm::raw_ostream &out);
  void PrintDefs(llvm::raw_ostream &out);
};
