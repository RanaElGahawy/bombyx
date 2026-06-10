#pragma once

#include "core/IR.hpp"
#include "clang/AST/ASTContext.h"
#include <cstdint>
#include <memory>
#include <set>
#include <unordered_map>
#include <variant>
#include <vector>

// ─── Type System ─────────────────────────────────────────────────────────────

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

// Inline helper — used in both analysis and printer TUs.
template <typename T> inline T *hctGetIf(HardCilkType *Ty) {
  return Ty ? std::get_if<T>(&Ty->V) : nullptr;
}
template <typename T> inline const T *hctGetIf(const HardCilkType *Ty) {
  return Ty ? std::get_if<T>(&Ty->V) : nullptr;
}

// ─── Task Info ───────────────────────────────────────────────────────────────

struct HCTaskInfo {
  std::set<IRFunction *> SendArgList;
  bool IsRoot = false;
  bool IsCont = false;
  bool IsSynthetic = false;
  bool GenerateArgOutWriteBuffer = false;
  bool HasAXI = false;
  uint32_t BufferedArgumentBits = 0;
  HardCilkBaseType BufferedArgType = TY_VOID;
  size_t TaskSize;
  size_t TaskPadding;
  std::unique_ptr<HardCilkType> RetTy;
  std::unordered_map<const StoreIRStmt *, uint32_t> BufferedStoreAllowMap;
  HCTaskInfo() {}
};

using TaskInfosTy = std::unordered_map<IRFunction *, HCTaskInfo>;

// ─── Analysis Result ─────────────────────────────────────────────────────────

struct HardCilkAnalysisResult {
  TaskInfosTy TaskInfos;
  // Owned synthetic base continuation — may be null if unused.
  // Its raw pointer appears as a key in TaskInfos; this unique_ptr keeps the
  // object alive for the lifetime of the result.
  std::unique_ptr<IRFunction> SyntheticBaseContinuation;
};

HardCilkAnalysisResult RunHardCilkAnalysis(IRProgram &P);

// ─── Shared Type Helpers (used by analysis and printer) ──────────────────────

HardCilkType *clangTypeToHardCilk(IRType &Ty);
HardCilkRecordType clangRecordTypeToHardCilk(const clang::RecordDecl *RD);

int hardCilkTypeSize(HardCilkBaseType Ty);
int hardCilkTypeSize(HardCilkType *Ty);

bool typeIsVoid(const HardCilkType &Ty);
