#pragma once

#include "core/OpenCilk2IR.hpp"
#include "hardcilk/HardCilkAnalysis.hpp"
#include "clang/AST/ASTContext.h"
#include <map>
#include <string>
#include <vector>

struct DriverSpec {
  std::string ClassName;
  std::string TaskStructName;
  size_t NumZeroFields = 0;
  bool HasPadding = false;
  bool HasContTask = false;
  std::string ContTaskStructName;
  size_t ContNumZeroFields = 0;
  std::vector<std::string> HelperFunctions;
  std::vector<std::string> ExternGlobals;
  std::vector<std::string> PreCallStmts;
  std::vector<std::string> PostCallStmts;

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

class VitisHLSTarget {
private:
  IRProgram &P;
  const std::string &AppName;
  const TaskInfosTy &TaskInfos;
  DriverCallersTy DriverCallers;
  bool ArgOutImplList[TY_LAST] = {false};
  std::vector<std::string> ExtraIncludes;
  std::map<std::string, unsigned> DataflowStartFifoDepth;

  void PrintDef(llvm::raw_ostream &Out, IRFunction *Task, HCTaskInfo &Info);

public:
  /// Select what to do about deep-state PEs (--deep-state=...). The argument is
  /// a deepstate::Mode; it is passed as an int so this header does not have to
  /// pull in the pass's own header.
  static void setDeepStateMode(int M);

private:

  DriverSpec BuildDriverSpec(clang::ASTContext &C);

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
  VitisHLSTarget(IRProgram &P, const std::string &AppName,
                 const HardCilkAnalysisResult &Analysis,
                 DriverCallersTy DriverCallers);

  void SetExtraIncludes(std::vector<std::string> Inc) {
    ExtraIncludes = std::move(Inc);
  }

  void PrintHardCilk(llvm::raw_ostream &out, clang::ASTContext &C);

  /// PE name -> the `config_dataflow -start_fifo_depth` its synthesis script
  /// must carry, for every PE PrintHardCilk emitted as a DATAFLOW region.
  ///
  /// Populated by PrintHardCilk and empty otherwise (in particular under
  /// --deep-state=none and --deep-state=latency), so it must be read after that
  /// call and handed to PrintVitisHLSArtifacts. There is no source-level pragma
  /// for the start-FIFO depth: leaving it at the Vitis default of 2 caps the
  /// region at two invocations in flight and costs ~10x throughput, so this is
  /// not an optional decoration -- a DATAFLOW PE whose TCL misses the directive
  /// is correct but not worth generating.
  const std::map<std::string, unsigned> &getDataflowStartFifoDepths() const {
    return DataflowStartFifoDepth;
  }
  void PrintDriver(llvm::raw_ostream &out);
  void PrintDefs(llvm::raw_ostream &out);
  void PrintDriverHeader(llvm::raw_ostream &out, clang::ASTContext &C);
};
