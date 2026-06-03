#pragma once

#include "IR.hpp"
#include "OpenCilk2IR.hpp"
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

struct DriverSpec {
  std::string ClassName;
  std::string TaskStructName;
  size_t NumZeroFields = 0;
  bool HasPadding = false;

  std::vector<std::string> HelperFunctions;
  std::vector<std::string> ExternGlobals;
  std::vector<std::string> PreCallStmts;
  std::vector<std::string> PostCallStmts;

  // Pointer args that need to be copied to and from the FPGA
  struct PtrArg {
    std::string BaseName;
    std::string PointeeType;
    std::string SizeExpr;
    bool CopyBack = false;
  };
  std::vector<PtrArg> PtrArgs;

  struct FieldAssign {
    std::string Field;
    std::string Value;
  };
  std::vector<FieldAssign> FieldAssignments;
};

class HardCilkTarget {
private:
  IRProgram &P;
  const std::string &AppName;
  TaskInfosTy TaskInfos;
  DriverCallersTy DriverCallers;
  std::unique_ptr<IRFunction> SyntheticBaseContinuation;
  bool ArgOutImplList[TY_LAST] = {false};

  void analyzeSendArguments();
  void analyzeArgOutWriteBuffers();
  IRFunction *ensureBaseContinuation();

  void PrintDef(llvm::raw_ostream &Out, IRFunction *Task, HCTaskInfo &Info);

  // Driver Analysis
  DriverSpec BuildDriverSpec(clang::ASTContext &C);

  // Driver Printing.
  void PrintStartSystem(llvm::raw_ostream &Out);
  void PrintManagementLoop(llvm::raw_ostream &Out);
  void PrintAllocateMemFPGA(llvm::raw_ostream &Out, const std::string &AddrVar,
                            const std::string &SizeExpr,
                            const std::string &Alignment);
  void PrintCopyToDevice(llvm::raw_ostream &Out, const std::string &Addr,
                         const std::string &Data, const std::string &SizeExpr);
  void PrintCopyFromDevice(llvm::raw_ostream &Out, const std::string &Data,
                           const std::string &Addr,
                           const std::string &SizeExpr);

public:
  HardCilkTarget(IRProgram &P, const std::string &AppName,
                 DriverCallersTy DriverCallers);

  void PrintHardCilk(llvm::raw_ostream &out, clang::ASTContext &C);
  void PrintDescJson(llvm::raw_ostream &out);
  void PrintDriver(llvm::raw_ostream &out);
  void PrintDefs(llvm::raw_ostream &out);
  void PrintDriverHeader(llvm::raw_ostream &out, clang::ASTContext &C);
};
