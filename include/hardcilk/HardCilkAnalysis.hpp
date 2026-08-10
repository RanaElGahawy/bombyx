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
  IRFuncSetTy SendArgList;
  uint8_t Tag = 0; // 8-bit continuation tag; meaningful only when IsCont.
  bool IsRoot = false;
  bool IsCont = false;
  bool IsSynthetic = false;
  // Task belongs to a `#pragma BOMBYX OVERLAP` loop (reentry / continuation /
  // exit). Mirrors IRFunction::Info.IsOverlap.
  bool IsOverlap = false;
  // This is an OVERLAP continuation: it is fed in-order by the generated
  // SystemVerilog merge wrapper via a plain task stream, so it must NOT emit the
  // memory-backed closure / allocator (closureIn) / argumentNotifier machinery.
  bool StreamingCont = false;
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

// Ordered by task name (IRFunctionNameLess), NOT by pointer. Emission order of
// PEs, task structs, `#define`s and descriptor entries all follow this map's
// iteration order, so a pointer-ordered container here makes bombyx's output
// vary run to run under ASLR. See IRFunctionNameLess in core/IR.hpp.
//
// std::map also has the practical advantage over unordered_map that inserting
// into TaskInfos never invalidates references to existing entries — several
// analysis passes hold `HCTaskInfo &` across insertions.
using TaskInfosTy = IRFuncMapTy<HCTaskInfo>;

// ─── OVERLAP wrapper grouping ────────────────────────────────────────────────

// Shared AXI data width (bits) pinned onto every memory-accessing PE collapsed
// into a `#pragma BOMBYX OVERLAP` wrapper. The sub-PEs are wired together by the
// generated wrapper, so their AXI masters must all agree on width; 256 is the
// HBM interface default. Emitted both as the HLS `max_widen_bitwidth` and as the
// wrapper's single shared `MEM_DATA_WIDTH` parameter.
static constexpr int kOverlapAXIDataWidth = 256;

// One collapsed OVERLAP subsystem: the PEs of a single `#pragma BOMBYX
// OVERLAP` loop, which the wrapper instantiates internally and which therefore
// do not appear as scheduler-visible tasks.
struct OverlapGroup {
  std::string WrapperName; // "<entry>_overlap_wrapper"
  IRFunction *Entry;       // task whose closure the wrapper is scheduled with
  IRFuncSetTy Internal;
  // True when the loop body escapes: some task in the chain dependently spawns
  // a real (non-memReader) task, so the wrapper cannot swallow the whole loop.
  // It then stands in for the *reentry* only — the loop initializer, the exit
  // and the continuation that consumes the real spawn's result stay external,
  // and the loop-back edge runs through the scheduler rather than an internal
  // FIFO. `Entry` is the reentry in that case, and the loop initializer in the
  // fully-internal case.
  bool Escapes = false;
  IRFunction *Reentry = nullptr;
  // Set only when Escapes: the task inside the wrapper that issues the real
  // dependent spawn, and that spawn's external continuation.
  IRFunction *EscapeIssuer = nullptr;
  IRFunction *EscapeCont = nullptr;
};

// Identify every collapsed OVERLAP subsystem in the app, one per OVERLAP loop
// (keyed on IRFunctionInfo::OverlapId). Each group's Internal set is that
// loop's IsOverlap tasks plus the memReader leaves they spawn *dependently* —
// a plain spawn of an unrelated task (e.g. `cilk_spawn countIntersections`
// from a loop body) stays external and keeps normal continuation mechanics.
std::vector<OverlapGroup> computeOverlapGroups(const TaskInfosTy &TaskInfos);

// Synthesised memory readers whose m_axi port may carry an `#pragma HLS cache`.
//
// Two conditions must both hold. The reader must show *spatial reuse*
// (IRFunctionInfo::SpatialReuseHint — two of its round's reads fall inside one
// AXI beat), so a cache actually earns its BRAM; and the pointer it reads must
// be provably never written anywhere on the FPGA, so a cached line can never go
// stale behind a write from another PE (a cross-PE RAW hazard).
//
// Read-only is decided over the whole IR program, which contains exactly the
// FPGA-side functions — host code marked `#pragma BOMBYX IGNORE` never reaches
// the IR. That is precisely the "cannot change after execution starts" scope: a
// buffer the host fills before launch and only reads afterwards qualifies.
IRFuncSetTy computeCacheableReaders(IRProgram &P,
                                    const TaskInfosTy &TaskInfos);

// Synthesised memory readers whose base pointer is provably never written
// anywhere on the FPGA — the pure memory-dependence half of
// computeCacheableReaders, with no spatial-reuse requirement. This is what
// decides the OVERLAP shared reader's per-site CACHE_EN bits: a per-context
// line cache is safe exactly when the site's data is read-only, whether or not
// two reads of one round share a beat.
IRFuncSetTy computeReadOnlyReaders(IRProgram &P);

// Convenience wrapper: the group containing F, or nullptr.
const OverlapGroup *findOverlapGroup(const std::vector<OverlapGroup> &Groups,
                                     IRFunction *F);

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
