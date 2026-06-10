#pragma once

#include "hardcilk/HardCilkAnalysis.hpp"
#include "core/OpenCilk2IR.hpp"
#include "clang/AST/ASTContext.h"
#include <string>
#include <vector>

struct DriverSpec {
  std::string ClassName;
  std::string TaskStructName;
  size_t NumZeroFields = 0;
  bool HasPadding = false;

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

  void PrintDef(llvm::raw_ostream &Out, IRFunction *Task, HCTaskInfo &Info);

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

  void PrintHardCilk(llvm::raw_ostream &out, clang::ASTContext &C);
  void PrintDriver(llvm::raw_ostream &out);
  void PrintDefs(llvm::raw_ostream &out);
  void PrintDriverHeader(llvm::raw_ostream &out, clang::ASTContext &C);
};
