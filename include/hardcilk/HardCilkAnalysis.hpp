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
  uint8_t Tag = 0; // 8-bit continuation tag; meaningful only when IsCont.
  bool IsRoot = false;
  bool IsCont = false;
  bool IsSynthetic = false;
  bool GenerateArgOutWriteBuffer = false;
  bool EmitFinalArgOutFlush = false;
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

// ─── Continuation closure write-buffer beats ─────────────────────────────────

// Maximum closure payload the continuation write buffer can transfer in a
// single beat. Closures wider than this are written in several ordered beats.
static constexpr unsigned MAX_CLOSURE_BEAT_BITS = 512;

// Number of ordered write-buffer beats needed to write this continuation's
// closure to memory (1 = fits in one beat, no split). The padded closure width
// is always a power of two, so a split (>1) always yields 64-byte beats.
inline unsigned closureWriteBeats(const HCTaskInfo &Info) {
  unsigned Bits = (unsigned)(Info.TaskSize + Info.TaskPadding) * 8;
  return (Bits + MAX_CLOSURE_BEAT_BITS - 1) / MAX_CLOSURE_BEAT_BITS;
}

// Bytes of closure data carried per beat (== 64 whenever beats > 1).
inline unsigned closureBeatBytes(const HCTaskInfo &Info) {
  return (unsigned)(Info.TaskSize + Info.TaskPadding) / closureWriteBeats(Info);
}

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
