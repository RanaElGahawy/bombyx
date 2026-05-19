#include "IR.hpp"
#include "util.hpp"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <deque>

#include <clang/AST/ASTContext.h>
#include <clang/AST/StmtCilk.h>
#include <memory>
#include <regex>

SymTable GSymTable;

const std::string &GetSym(Sym S) { return GSymTable.Table[S]; }

Sym PutSym(std::string Name) {
  int NameCnt = -1;
  auto It = GSymTable.DupCnt.find(Name);
  if (It != GSymTable.DupCnt.end())
    NameCnt = (int)It->second;

  if (NameCnt < 0) {
    GSymTable.DupCnt[Name] = 0;
    GSymTable.Table.push_back(Name);
  } else {
    // Find a free name that doesn't collide with others
    std::string Candidate;
    int Ctr = NameCnt;
    do {
      Candidate = Name + std::to_string(Ctr++);
    } while (GSymTable.DupCnt.count(Candidate) ||
             GSymTable.Reserved.count(Candidate));
    GSymTable.DupCnt[Name] = Ctr;
    GSymTable.DupCnt[Candidate] = 0;
    GSymTable.Table.push_back(Candidate);
  }
  return GSymTable.Table.size() - 1;
}

void ReserveName(const std::string &Name) { GSymTable.Reserved.insert(Name); }

/////////////
// IRExpr //
///////////

void IndexIRExpr::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(Ind && Arr);
  Ctx.ExprCB(&Ctx, Out, Arr.get());
  Out << "[";
  Ctx.ExprCB(&Ctx, Out, Ind.get());
  Out << "]";
}

IRExpr *IndexIRExpr::clone() {
  assert(Ind);
  IRLvalExpr *NewArr = dyn_cast<IRLvalExpr>(Arr->clone());
  IRExpr *NewInd = Ind->clone();
  return new IndexIRExpr(NewArr, NewInd, ArrType);
}

void RefIRExpr::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(E);
  Out << "&(";
  Ctx.ExprCB(&Ctx, Out, E.get());
  Out << ")";
}

IRExpr *RefIRExpr::clone() {
  assert(E);
  IRExpr *NewE = E->clone();
  return new RefIRExpr(NewE);
}

void DRefIRExpr::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(Expr);
  Out << "*(";
  Ctx.ExprCB(&Ctx, Out, Expr.get());
  Out << ")";
}

IRExpr *DRefIRExpr::clone() {
  assert(Expr);
  IRExpr *NewE = Expr->clone();
  return new DRefIRExpr(NewE, PointeeType);
}

void AccessIRExpr::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(Base);
  if (auto *IE = llvm::dyn_cast<IdentIRExpr>(Base.get())) {
    Ctx.IdentCB(Out, IE->Ident);
    Out << (Arrow ? "->" : ".");
  } else if (auto *DE = llvm::dyn_cast<DRefIRExpr>(Base.get())) {
    Ctx.ExprCB(&Ctx, Out, DE->Expr.get());
    Out << "->";
  } else {
    Ctx.ExprCB(&Ctx, Out, Base.get());
    Out << (Arrow ? "->" : ".");
  }
  Out << Field;
}

IRExpr *AccessIRExpr::clone() {
  assert(Base);
  return new AccessIRExpr(llvm::dyn_cast<IRLvalExpr>(Base->clone()), Field,
                          Arrow);
}

void IdentIRExpr::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(Ident);
  Ctx.IdentCB(Out, Ident);
}

IRExpr *IdentIRExpr::clone() {
  assert(Ident);
  return new IdentIRExpr(Ident);
}

/*void SymVarIRExpr::print(llvm::raw_ostream &Out, IRPrintContext &Ctx){
  Out << "sv" << SymVar;
}

IRExpr* SymVarIRExpr::clone() {
  return new SymVarIRExpr(SymVar);
}*/

void FIdentIRExpr::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  if (auto *F = std::get_if<IRFunction *>(&FR)) {
    Out << (*F)->getName();
  } else {
    Out << std::get<ASTVarRef>(FR)->getQualifiedNameAsString();
  }
}

IRExpr *FIdentIRExpr::clone() {
  if (auto *F = std::get_if<IRFunction *>(&FR)) {
    return new FIdentIRExpr(*F);
  } else {
    return new FIdentIRExpr(std::get<ASTVarRef>(FR));
  }
}

// ASTLiteralIRExpr::print is defined after VarRenamePrinterHelper below.

IRExpr *ASTLiteralIRExpr::clone() {
  assert(Lit);
  return new ASTLiteralIRExpr(Lit);
}

void BinopIRExpr::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(Left && Right);
  Out << "(";
  Ctx.ExprCB(&Ctx, Out, Left.get());
  Out << " ";
  printBinop(Out);
  Out << " ";
  Ctx.ExprCB(&Ctx, Out, Right.get());
  Out << ")";
}

IRExpr *BinopIRExpr::clone() {
  assert(Left && Right);
  IRExpr *NewLeft = Left->clone();
  IRExpr *NewRight = Right->clone();
  return new BinopIRExpr(Op, NewLeft, NewRight);
}

void UnopIRExpr::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(Expr);
  Out << "(";
  const char *Op;
  if (printUnop(Op)) {
    Ctx.ExprCB(&Ctx, Out, Expr.get());
    Out << Op;
  } else {
    Out << Op;
    Ctx.ExprCB(&Ctx, Out, Expr.get());
  }
  Out << ")";
}

IRExpr *UnopIRExpr::clone() {
  assert(Expr);
  IRExpr *NewE = Expr->clone();
  return new UnopIRExpr(Op, NewE);
}

void ISpawnIRExpr::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  Out << "spawn ";
  if (auto *F = std::get_if<IRFunction *>(&Fn)) {
    Out << (*F)->getName();
  } else {
    Out << std::get<ASTVarRef>(Fn)->getName();
  }
  Out << "(";
  bool first = true;
  for (auto &Arg : Args) {
    if (first) {
      first = false;
    } else {
      Out << ",";
    }
    Ctx.ExprCB(&Ctx, Out, Arg.get());
  }
  Out << ")";
}

IRExpr *ISpawnIRExpr::clone() {
  std::vector<IRExpr *> NewArgs;
  for (auto &Arg : Args) {
    NewArgs.push_back(Arg->clone());
  }
  if (auto *F = std::get_if<IRFunction *>(&Fn)) {
    return new ISpawnIRExpr(*F, NewArgs);
  } else {
    return new ISpawnIRExpr(std::get<ASTVarRef>(Fn), NewArgs);
  }
}

void CallIRExpr::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  if (auto *F = std::get_if<IRFunction *>(&Fn)) {
    Out << (*F)->getName();
  } else {
    Out << std::get<ASTVarRef>(Fn)->getQualifiedNameAsString();
  }
  Out << "(";
  bool first = true;
  for (auto &Arg : Args) {
    if (first) {
      first = false;
    } else {
      Out << ",";
    }
    Ctx.ExprCB(&Ctx, Out, Arg.get());
  }
  Out << ")";
}

IRExpr *CallIRExpr::clone() {
  std::vector<IRExpr *> NewArgs;
  for (auto &Arg : Args) {
    NewArgs.push_back(Arg->clone());
  }
  if (auto *F = std::get_if<IRFunction *>(&Fn)) {
    return new CallIRExpr(*F, NewArgs);
  } else {
    return new CallIRExpr(std::get<ASTVarRef>(Fn), NewArgs);
  }
}

void CastIRExpr::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  Out << "((";
  CastType.print(Out, Ctx.ASTCtx.getPrintingPolicy());
  Out << ") ";
  Ctx.ExprCB(&Ctx, Out, E.get());
  Out << ")";
}

IRExpr *CastIRExpr::clone() { return new CastIRExpr(CastType, E->clone()); }

/////////////
// IRStmt //
///////////
void LoopIRStmt::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  if (Inc || Init) {
    Out << "for (";
    if (Init) {
      Init->print(Out, Ctx);
    }
    Out << ";";
    Ctx.ExprCB(&Ctx, Out, Cond.get());
    Out << ";";
    if (Inc) {
      Inc->print(Out, Ctx);
    }
    Out << ")";
  } else {
    Out << "while (";
    Ctx.ExprCB(&Ctx, Out, Cond.get());
    Out << ")";
  }
}

IRStmt *LoopIRStmt::clone() {
  assert(Cond);
  IRExpr *NewCond = Cond->clone();
  IRStmt *NewInc = Inc ? Inc->clone() : nullptr;
  IRStmt *NewInit = Init ? Init->clone() : nullptr;
  return new LoopIRStmt(NewCond, NewInc, NewInit);
}

void IfIRStmt::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(Cond);
  Out << "if (";
  Ctx.ExprCB(&Ctx, Out, Cond.get());
  Out << ")";
}

IRStmt *IfIRStmt::clone() {
  assert(Cond);
  IRExpr *NewCond = Cond->clone();
  return new IfIRStmt(NewCond);
}

void SpawnNextIRStmt::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(Fn);
  Out << "spawnNext ";
  Out << Fn->getName();
}

IRStmt *SpawnNextIRStmt::clone() {
  assert(Fn);
  return new SpawnNextIRStmt(Fn);
}

void ESpawnIRStmt::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  if (Dest) {
    Out << "espawn @";
    Ctx.ExprCB(&Ctx, Out, Dest.get());
    Out << " ";
  } else {
    Out << "espawn ";
  }
  Out << Fn->getName();
  Out << "(";
  bool first = true;
  for (auto &Arg : Args) {
    if (first) {
      first = false;
    } else {
      Out << ",";
    }
    Ctx.ExprCB(&Ctx, Out, Arg.get());
  }
  Out << ")";
  if (SN) {
    Out << "[" << SN->Fn->getName() << "]";
  }
}

IRStmt *ESpawnIRStmt::clone() {
  std::vector<IRExpr *> NewArgs;
  for (auto &Arg : Args) {
    NewArgs.push_back(Arg->clone());
  }

  IRLvalExpr *ND = Dest ? (dyn_cast<IRLvalExpr>(Dest->clone())) : nullptr;
  return new ESpawnIRStmt(ND, Fn, SN, NewArgs, Local);
}

void ExprWrapIRStmt::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(Expr);
  Ctx.ExprCB(&Ctx, Out, Expr.get());
}

IRStmt *ExprWrapIRStmt::clone() {
  assert(Expr);
  return new ExprWrapIRStmt(Expr->clone());
}

namespace {
struct VarRenamePrinterHelper : clang::PrinterHelper {
  const std::unordered_map<const clang::NamedDecl *, std::string> &Renames;
  clang::PrintingPolicy PP;
  VarRenamePrinterHelper(
      const std::unordered_map<const clang::NamedDecl *, std::string> &R,
      clang::PrintingPolicy PP)
      : Renames(R), PP(PP) {}
  bool handledStmt(clang::Stmt *E, llvm::raw_ostream &OS) override {
    if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(E)) {
      auto It = Renames.find(DRE->getDecl());
      if (It != Renames.end()) {
        OS << It->second;
        return true;
      }
    }
    // For lambda expressions, rewrite any captured variable that has been
    // renamed (e.g. end -> largs->end) as an init-capture [end = largs->end]
    if (auto *Lambda = llvm::dyn_cast<clang::LambdaExpr>(E)) {
      bool AnyRenamed = false;
      for (const auto &Cap : Lambda->captures()) {
        if (Cap.capturesVariable() && Renames.count(Cap.getCapturedVar()) > 0) {
          AnyRenamed = true;
          break;
        }
      }
      if (!AnyRenamed)
        return false;
      // Print capture list with init-captures for renamed vars.
      OS << "[";
      bool First = true;
      if (Lambda->getCaptureDefault() == clang::LCD_ByRef)
        OS << "&";
      else if (Lambda->getCaptureDefault() == clang::LCD_ByCopy)
        OS << "=";
      else
        First = true; // no default
      if (Lambda->getCaptureDefault() != clang::LCD_None)
        First = false;
      for (const auto &Cap : Lambda->captures()) {
        if (Cap.isImplicit())
          continue;
        if (!First)
          OS << ", ";
        First = false;
        if (Cap.capturesVariable()) {
          auto *VD = Cap.getCapturedVar();
          auto It = Renames.find(VD);
          if (It != Renames.end()) {
            OS << VD->getName() << " = " << It->second;
          } else {
            if (Cap.getCaptureKind() == clang::LCK_ByRef)
              OS << "&";
            OS << VD->getName();
          }
        } else if (Cap.capturesThis()) {
          if (Cap.getCaptureKind() == clang::LCK_StarThis)
            OS << "*this";
          else
            OS << "this";
        }
      }
      OS << "]";
      // Print parameter list.
      auto *CallOp = Lambda->getCallOperator();
      OS << "(";
      bool FirstParam = true;
      for (auto *Param : CallOp->parameters()) {
        if (!FirstParam)
          OS << ", ";
        FirstParam = false;
        Param->getType().print(OS, PP);
        if (!Param->getName().empty())
          OS << " " << Param->getName();
      }
      OS << ") {\n";
      // Print body — no helper needed: captured vars inside the body are
      // fresh lambda-scope VarDecls named after the original, so printPretty
      // produces the right names without renaming.
      Lambda->getBody()->printPretty(OS, nullptr, PP);
      OS << "}";
      return true;
    }
    return false;
  }
};
} // namespace

// Recursively rewriting every ReturnStmt encountered as
//   SEND_ARGUMENT(ContKey, val);
// Uses printPretty for statements that don't contain returns,
static void printStmtWithReturnRewrite(llvm::raw_ostream &Out, clang::Stmt *S,
                                       VarRenamePrinterHelper *Helper,
                                       const clang::PrintingPolicy &PP,
                                       const std::string &ContKey,
                                       unsigned Depth) {
  const unsigned IW = PP.Indentation ? PP.Indentation : 2;
  std::string Ind(Depth * IW, ' ');

  if (auto *RS = clang::dyn_cast<clang::ReturnStmt>(S)) {
    Out << Ind << "SEND_ARGUMENT(" << ContKey << ", ";
    if (auto *Val = RS->getRetValue())
      Val->printPretty(Out, Helper, PP);
    else
      Out << "0";
    Out << ");\n" << Ind << "return;\n";
    return;
  }

  if (auto *SW = clang::dyn_cast<clang::SwitchStmt>(S)) {
    Out << Ind << "switch (";
    SW->getCond()->printPretty(Out, Helper, PP);
    Out << ") {\n";
    auto *Body = clang::dyn_cast<clang::CompoundStmt>(SW->getBody());
    if (Body) {
      for (auto *Child : Body->body())
        printStmtWithReturnRewrite(Out, Child, Helper, PP, ContKey, Depth + 1);
    } else if (SW->getBody()) {
      printStmtWithReturnRewrite(Out, SW->getBody(), Helper, PP, ContKey,
                                 Depth + 1);
    }
    Out << Ind << "}";
    return;
  }

  if (auto *CS = clang::dyn_cast<clang::CompoundStmt>(S)) {
    Out << "{\n";
    for (auto *Child : CS->body())
      printStmtWithReturnRewrite(Out, Child, Helper, PP, ContKey, Depth + 1);
    Out << Ind << "}\n";
    return;
  }

  if (auto *Ca = clang::dyn_cast<clang::CaseStmt>(S)) {
    std::string CaseInd(Depth > 0 ? (Depth - 1) * IW : 0, ' ');
    Out << CaseInd << "case ";
    Ca->getLHS()->printPretty(Out, Helper, PP);
    Out << ":\n";
    printStmtWithReturnRewrite(Out, Ca->getSubStmt(), Helper, PP, ContKey,
                               Depth);
    return;
  }

  if (auto *Df = clang::dyn_cast<clang::DefaultStmt>(S)) {
    std::string DefInd(Depth > 0 ? (Depth - 1) * IW : 0, ' ');
    Out << DefInd << "default:\n";
    printStmtWithReturnRewrite(Out, Df->getSubStmt(), Helper, PP, ContKey,
                               Depth);
    return;
  }

  if (clang::isa<clang::Expr>(S)) {
    Out << Ind;
    S->printPretty(Out, Helper, PP, 0);
    Out << ";\n";
    return;
  }

  S->printPretty(Out, Helper, PP, Depth);
}

void ASTLiteralIRExpr::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(Lit);
  if (Ctx.VarRenames.empty()) {
    Lit->printPretty(Out, nullptr, Ctx.ASTCtx.getPrintingPolicy());
  } else {
    VarRenamePrinterHelper Helper(Ctx.VarRenames,
                                  Ctx.ASTCtx.getPrintingPolicy());
    Lit->printPretty(Out, &Helper, Ctx.ASTCtx.getPrintingPolicy());
  }
}

void ASTStmtWrapIRStmt::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(S);
  auto Effective = VarRenames;
  for (auto &[D, N] : Ctx.VarRenames)
    Effective[D] = N;
  const auto &PP = Ctx.ASTCtx.getPrintingPolicy();
  // rewrite returns
  if (!Ctx.TaskContinuationKey.empty()) {
    VarRenamePrinterHelper Helper(Effective, PP);
    printStmtWithReturnRewrite(Out, S, &Helper, PP, Ctx.TaskContinuationKey, 0);
    // no rewriting needed
  } else if (Effective.empty()) {
    S->printPretty(Out, nullptr, PP);
  } else {
    // rename variables
    VarRenamePrinterHelper Helper(Effective, PP);
    S->printPretty(Out, &Helper, PP);
  }
}

IRStmt *ASTStmtWrapIRStmt::clone() {
  return new ASTStmtWrapIRStmt(S, VarRenames);
}

void StoreIRStmt::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(Dest);

  Ctx.ExprCB(&Ctx, Out, Dest.get());
  Out << " = ";
  Ctx.ExprCB(&Ctx, Out, Src.get());
}

IRStmt *StoreIRStmt::clone() {
  assert(Dest && Src);
  IRLvalExpr *NewDest = dyn_cast<IRLvalExpr>(Dest->clone());
  IRExpr *NewSrc = Src->clone();
  return new StoreIRStmt(NewDest, NewSrc);
}

void CopyIRStmt::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  assert(Dest);
  Ctx.IdentCB(Out, Dest);
  Out << " = ";
  Ctx.ExprCB(&Ctx, Out, Src.get());
}

IRStmt *CopyIRStmt::clone() {
  assert(Dest && Src);
  IRExpr *NewSrc = Src->clone();
  return new CopyIRStmt(Dest, NewSrc);
}

void SyncIRStmt::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  Out << "sync";
}

IRStmt *SyncIRStmt::clone() { return new SyncIRStmt(); }

void BreakIRStmt::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  Out << "break";
}

IRStmt *BreakIRStmt::clone() { return new BreakIRStmt(); }

void ClosureDeclIRStmt::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  Out << "cdef ";
  Out << Fn->getName();
  Out << "(";
  for (auto &[src, dst] : Caller2Callee) {
    Out << " " << GetSym(src->Name);
  }
  Out << " )";
  if (SpawnCount) {
    Out << " // SC <- ";
    Ctx.ExprCB(&Ctx, Out, SpawnCount.get());
  }
}

IRStmt *ClosureDeclIRStmt::clone() {
  auto *C = new ClosureDeclIRStmt(Fn);
  for (auto &[Src, Dst] : Caller2Callee) {
    C->addCallerToCaleeVarMapping(Src, Dst);
  }
  if (SpawnCount) {
    C->annotateSpawnCount(SpawnCount->clone());
  }
  return C;
}

void ReturnIRStmt::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  Out << "return ";
  if (RetVal) {
    Ctx.ExprCB(&Ctx, Out, RetVal.get());
  }
}

IRStmt *ReturnIRStmt::clone() {
  IRExpr *NewRetVal = RetVal ? RetVal->clone() : nullptr;
  return new ReturnIRStmt(NewRetVal);
}

void ScopeAnnotIRStmt::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {}

IRStmt *ScopeAnnotIRStmt::clone() { return new ScopeAnnotIRStmt(SA); }

// void IRStmt::printAllIdentifiers() {
//   for (auto it = ExprIdentifierIterator(innerStmt); !it.done(); ++it) {
//     llvm::outs() << (*it)->getNameInfo().getAsString() << "\n";
//   }
// }

///////////////////
// IRBasicBlock //
/////////////////

void IRBasicBlock::iteratePreds(std::function<void(IRBasicBlock *B)> CB) {
  for (auto &B : *Parent) {
    auto &BSuccs = (B.get())->Succs;
    if (BSuccs.contains(this)) {
      CB(B.get());
    }
  }
}

void IRBasicBlock::clone(IRBasicBlock *Dest) {
  for (auto &Stmt : Stmts) {
    auto *Cloned = Stmt->clone();
    if (Stmt->Silent)
      Cloned->setSilent();
    Dest->pushStmtBack(Cloned);
  }
  if (Term != nullptr) {
    Dest->Term = dyn_cast<IRTerminatorStmt>(Term->clone());
  }
}

IRBasicBlock *IRBasicBlock::splitAt(int Index) {
  assert(Index >= 0 && Index <= Stmts.size());
  auto *RBlock = Parent->createBlock();
  for (int i = Index; i < Stmts.size(); i++) {
    RBlock->pushStmtBack(Stmts[i].release());
  }
  if (Term) {
    RBlock->Term = Term;
    Term = nullptr;
  }
  Stmts.resize(Index);
  return RBlock;
}

IRStmt *IRBasicBlock::getAt(int Index) {
  assert(Index >= 0 && Index < Stmts.size());
  return Stmts[Index].get();
}

void IRBasicBlock::removeAt(int Index) {
  assert(Index >= 0 && Index < Stmts.size());
  auto it = begin() + Index;
  Stmts.erase(it);
}

void IRBasicBlock::insertAt(int Index, IRStmt *S) {
  assert(Index >= 0 && Index <= Stmts.size());
  auto it = begin() + Index;
  Stmts.insert(it, std::unique_ptr<IRStmt>(S));
}

size_t IRBasicBlock::lenInsns() const { return Stmts.size(); }

void IRBasicBlock::print(llvm::raw_ostream &Out, IRPrintContext &Ctx) {
  int I = 1;
  int j = 0;

  std::string OutS;
  llvm::raw_string_ostream OutOS(OutS);

  llvm::raw_ostream *OutT = Ctx.GraphVizEscapeChars ? &OutOS : &Out;
  for (auto &Stmt : Stmts) {
    *OutT << "   " << I << ": ";
    Stmt->print(*OutT, Ctx);
    *OutT << Ctx.NewlineSymbol;

    I++;
  }
  if (Term) {
    *OutT << "   T: ";
    Term->print(*OutT, Ctx);
    *OutT << Ctx.NewlineSymbol;
  }

  if (Ctx.GraphVizEscapeChars) {
    //|(\\[^l])
    std::regex re("([\"<>]|\\\\n)");
    OutS = std::regex_replace(OutS, re, "\\$1");
    Out << OutS;
  }
}

void IRBasicBlock::dumpGraph(llvm::raw_ostream &Out,
                             clang::ASTContext &Context) {
  Out << "\"{ [B" << getInd();
  Out << "]\\l";
  IRPrintContext Ctx = IRPrintContext{
      .ASTCtx = Context, .NewlineSymbol = "\\l", .GraphVizEscapeChars = true};
  print(Out, Ctx);
  Out << "}\"";
}

/////////////////
// IRFunction //
///////////////

IRBasicBlock *IRFunction::createBlock() {
  IRBlockPtr B = std::make_unique<IRBasicBlock>(Blocks.size(), this);
  IRBasicBlock *Bp = B.get();
  Blocks.push_back(std::move(B));
  return Bp;
}

void IRFunction::moveBlock(IRBasicBlock *B, IRFunction *Dest) {
  IRFunction::iterator BlockIt = begin();
  std::advance(BlockIt, B->getInd());
  IRBlockPtr OwnedB = std::move(*BlockIt);
  Blocks.erase(BlockIt);
  int I = 0;
  for (auto &MyB : Blocks) {
    MyB->Ind = I;
    I++;
  }
  B->Ind = Dest->Blocks.size();
  Dest->Blocks.push_back(std::move(OwnedB));
  B->Parent = Dest;
}

void IRFunction::cleanVars() {
  std::set<IRVarRef> accessed;
  std::set<std::string> opaqueNames;

  for (auto &B : *this) {
    auto VisitF = [&](auto &VR, bool lhs) { accessed.insert(VR); };
    for (auto &S : *B) {
      ExprIdentifierVisitor _(S.get(), VisitF);
      if (auto *ASW = dyn_cast<ASTStmtWrapIRStmt>(S.get())) {
        for (auto &[ND, Name] : ASW->VarRenames)
          opaqueNames.insert(Name);
      }
    }
    if (B->Term) {
      ExprIdentifierVisitor _(B->Term, VisitF);
    }
  }

  auto it = Vars.begin();
  while (it != Vars.end()) {
    auto *VR = &(*it);
    bool usedInOpaque = opaqueNames.count(GetSym(VR->Name));
    if (accessed.find(VR) == accessed.end() && !usedInOpaque &&
        VR->DeclLoc != IRVarDecl::ARG) {
      auto itc = it;
      it++;
      Vars.erase(itc);
    } else {
      it++;
    }
  }
}

void IRFunction::print(llvm::raw_ostream &out, clang::ASTContext &Context) {
  int i = 0;
  for (auto &B : Blocks) {
    fprintf(stdout, BHGREEN "Block %d" COLOR_RESET "\n", i);
    out << "PREDS: ";
    B->iteratePreds(
        [&](IRBasicBlock *Pred) -> void { out << Pred->getInd() << " "; });
    out << "\n";
    IRPrintContext Ctx =
        IRPrintContext{.ASTCtx = Context, .NewlineSymbol = "\n"};
    B->print(out, Ctx);
    out << "SUCCS: ";
    for (auto *Succ : B->Succs) {
      out << Succ->getInd() << " ";
    }
    out << "\n\n\n";
    i++;
  }
}

void IRFunction::dumpGraph(llvm::raw_ostream &out, clang::ASTContext &Context) {
  out << "subgraph clusterfn" << Ind;
  out << "{\nlabel=\"" << getName() << "\"\n";

  for (auto &B : Blocks) {
    auto *BB = B.get();
    out << "    Node" << Ind << "_" << BB->getInd();
    out << " [shape=record,";
    if (BB->Ind == 0) {
      out << "fontcolor=\"blue\",color=\"blue\",";
    } /* else if (BB == BB->Parent->Exit) {
       out << "fontcolor=\"green\",color=\"green\",";
     }*/
    out << "label=";
    BB->dumpGraph(out, Context);
    out << " ];\n";
  }

  for (auto &B : Blocks) {
    for (auto &Succ : B.get()->Succs) {
      out << "    Node" << Ind << "_" << B.get()->getInd();
      out << " -> Node" << Ind << "_" << Succ->getInd();
      out << ";\n";
    }
    /*
    const IRBasicBlock *HasSpawnToSpawnNext = nullptr;
    for (auto &S : *B) {
      if ((Spawn2SpawnNext.find(S.get()) != Spawn2SpawnNext.end())) {
        HasSpawnToSpawnNext = Spawn2SpawnNext[S.get()];
        break;
      }
    }
    if (HasSpawnToSpawnNext) {
      out << "    \"Node" << Ind << "_" << B->getInd();
      out << "\" -> \"Node" << Ind << "_" << HasSpawnToSpawnNext->getInd()
          << "\"";
      out << "  [style=\"dashed\" color=\"green\"];\n";
    }*/
  }
  out << "}\n";
}

/*
void IRFunction::dumpArgs(llvm::raw_ostream &out) {
  out << "\tArgs: ";
  for (auto *v : Args) {
    out << v->getName() << ", ";
  }
  out << "\n\tLocals: ";
  for (auto *v : Locals) {
    out << v->getName() << ", ";
  }
  if (!Materialized.empty()) {
    out << "\n\tMaterialized: ";
    for (auto *v : Materialized) {
      out << v->getName() << ", ";
    }
  }
  out << "\n";
}*/

////////////////
// IRPRogram //
//////////////

IRFunction *IRProgram::createFunc(const std::string &Name, IRType Ret) {
  IRFuncPtr F = std::make_unique<IRFunction>(Funcs.size(), Name, Ret, this);
  IRFunction *Fp = F.get();
  Funcs.push_back(std::move(F));
  return Fp;
}

void IRProgram::print(llvm::raw_ostream &out, clang::ASTContext &Context) {
  for (auto &F : Funcs) {
    F.get()->print(out, Context);
  }
}

void IRProgram::dumpGraph(llvm::raw_ostream &out, clang::ASTContext &Context) {
  out << "digraph unnamed {\ncompound=true;\n";
  for (auto &F : Funcs) {
    F->dumpGraph(out, Context);
  }
  // for (auto &F : Funcs) {
  //   for (const auto &[B, SnD] : F->SpawnNext2Cont) {
  //     out << "    \"Node" << F->Ind << "_" << B->getInd();
  //     out << "\" -> \"Node" << SnD->Ind << "_" << 0 << "\"";
  //     out << "  [style=\"dashed\" color=\"red\" lhead=clusterfn";
  //     out << SnD->Ind << "];\n";
  //   }
  // }
  out << "}\n";
}

////////////////////////
// ScopedIRTraverser //
//////////////////////

IRBasicBlock *FindJoin(IRBasicBlock *Left, IRBasicBlock *Right) {
  std::vector<IRBasicBlock *> WorkList;
  std::unordered_map<IRBasicBlock *, bool> Seen;

  WorkList.push_back(Left);
  while (!WorkList.empty()) {
    auto *B = WorkList.back();
    WorkList.pop_back();

    if (Seen.find(B) != Seen.end())
      continue;
    Seen[B] = true;
    for (auto *Succ : B->Succs) {
      WorkList.push_back(Succ);
    }
  }

  WorkList.push_back(Right);
  while (!WorkList.empty()) {
    auto *B = WorkList.back();
    WorkList.pop_back();

    if (Seen.find(B) != Seen.end()) {
      if (Seen[B]) {
        return B;
      } else {
        continue;
      }
    }
    Seen[B] = false;
    for (auto *Succ : B->Succs) {
      WorkList.push_back(Succ);
    }
  }
  return nullptr;
}

static IRBasicBlock *FindIfJoin(IRBasicBlock *ThenB, IRBasicBlock *ElseB) {
  // Compute blocks reachable from ElseB without going through ThenB.
  // This avoids false join candidates caused by loop back-edges: a block
  // inside ThenB's subgraph may appear reachable from ElseB via a loop cycle,
  // but that is not a valid join for the if-statement.
  std::unordered_map<IRBasicBlock *, bool> ElseDirectReachable;
  {
    std::deque<IRBasicBlock *> WL;
    WL.push_back(ElseB);
    while (!WL.empty()) {
      auto *B = WL.front();
      WL.pop_front();
      if (B == ThenB || ElseDirectReachable.count(B))
        continue;
      ElseDirectReachable[B] = true;
      for (auto *S : B->Succs)
        WL.push_back(S);
    }
  }

  // Compute blocks reachable from ThenB without going through ElseB.
  std::unordered_map<IRBasicBlock *, bool> ThenDirectReachable;
  {
    std::deque<IRBasicBlock *> WL;
    WL.push_back(ThenB);
    while (!WL.empty()) {
      auto *B = WL.front();
      WL.pop_front();
      if (B == ElseB || ThenDirectReachable.count(B))
        continue;
      ThenDirectReachable[B] = true;
      for (auto *S : B->Succs)
        WL.push_back(S);
    }
  }

  // BFS from ThenB; return the first block also in ElseDirectReachable.
  // (ElseB itself is a valid result — it means ThenB falls into ElseB
  // directly.)
  {
    std::unordered_map<IRBasicBlock *, bool> Seen;
    std::deque<IRBasicBlock *> Q;
    Q.push_back(ThenB);
    Seen[ThenB] = true;
    while (!Q.empty()) {
      auto *B = Q.front();
      Q.pop_front();
      if (B != ThenB && ElseDirectReachable.count(B))
        return B;
      for (auto *S : B->Succs) {
        if (!Seen.count(S)) {
          Seen[S] = true;
          Q.push_back(S);
        }
      }
    }
  }

  // Symmetric: BFS from ElseB; return the first block also in
  // ThenDirectReachable. Exclude both endpoints to prevent returning ThenB or
  // ElseB themselves.
  {
    std::unordered_map<IRBasicBlock *, bool> Seen;
    std::deque<IRBasicBlock *> Q;
    Q.push_back(ElseB);
    Seen[ElseB] = true;
    while (!Q.empty()) {
      auto *B = Q.front();
      Q.pop_front();
      if (B != ElseB && B != ThenB && ThenDirectReachable.count(B))
        return B;
      for (auto *S : B->Succs) {
        if (!Seen.count(S)) {
          Seen[S] = true;
          Q.push_back(S);
        }
      }
    }
  }

  return nullptr;
}

void ScopedIRTraverser::traverse(IRFunction &F) {
  WorkList.push_back(WorkItem(F.getEntry()));

  while (!WorkList.empty()) {
    auto W = WorkList.back();
    WorkList.pop_back();

    if (W.B) {
      assert(W.SE == None);
      auto B = W.B;

      int JC = 1;
      if (JoinCounts.find(B) != JoinCounts.end()) {
        JC = JoinCounts[B];
      }
      JC--;
      JoinCounts[B] = JC;

      if (JC != 0) {
        continue;
      }

      visitBlock(B);

      if (B->Term && isa<IfIRStmt>(B->Term)) {
        auto *ThenB = B->Succs[0];
        auto *ElseB = B->Succs[1];
        auto *JoinB = FindIfJoin(ThenB, ElseB);
        assert(JoinB != ThenB);
        if (JoinB) {
          WorkList.push_back(WorkItem(JoinB));
          JoinCounts[JoinB] = 2;
        }
        WorkList.push_back(WorkItem(Close));
        if (ElseB && ElseB != JoinB) {
          WorkList.push_back(WorkItem(ElseB));
          WorkList.push_back(WorkItem(Else));
          if (JoinB) {
            JoinCounts[JoinB] += 1;
          }
        }
        WorkList.push_back(WorkItem(ThenB));
        WorkList.push_back(WorkItem(Open));
      } else if (B->Term && isa<LoopIRStmt>(B->Term)) {
        auto *BodyB = B->Succs[0];
        auto *AfterB = B->Succs[1];
        // The common successor of the body and the loop itself should be the
        // loop.
        assert(FindJoin(BodyB, B) == B);

        // we don't need a join count for AfterB because
        // it will be looped back already
        WorkList.push_back(WorkItem(AfterB));
        WorkList.push_back(WorkItem(Close));
        WorkList.push_back(WorkItem(BodyB));
        WorkList.push_back(WorkItem(Open));
      } else {
        assert(B->Succs.size() <= 1);
        for (auto *Succ : B->Succs) {
          WorkList.push_back(WorkItem(Succ));
        }
      }
    } else {
      assert(W.SE != None);
      handleScope(W.SE);
    }
  }
}

void printFullIRProgram(llvm::raw_ostream &Out, IRProgram &P,
                        clang::ASTContext &Context) {
  IRPrintContext Ctx{
      .ASTCtx = Context, .NewlineSymbol = "\n", .GraphVizEscapeChars = false};

  Out << "========== IR Program ==========\n";

  for (auto &FPtr : P) {
    IRFunction *F = FPtr.get();
    Out << "\nfunction " << F->getName() << " {\n";

    Out << "  vars:\n";
    for (auto &V : F->Vars) {
      Out << "    ";
      switch (V.DeclLoc) {
      case IRVarDecl::ARG:
        Out << "[arg] ";
        break;
      case IRVarDecl::LOCAL:
        Out << "[local] ";
        break;
      default:
        Out << "[var] ";
        break;
      }
      Out << GetSym(V.Name) << "\n";
    }

    Out << "\n  blocks:\n";
    for (auto &BPtr : *F) {
      IRBasicBlock *B = BPtr.get();

      Out << "  B" << B->getInd() << ":\n";

      Out << "    preds: ";
      bool firstPred = true;
      B->iteratePreds([&](IRBasicBlock *Pred) {
        if (!firstPred)
          Out << ", ";
        Out << "B" << Pred->getInd();
        firstPred = false;
      });
      if (firstPred)
        Out << "(none)";
      Out << "\n";

      int stmtNo = 1;
      for (auto &S : *B) {
        Out << "    " << stmtNo++ << ": ";
        S->print(Out, Ctx);
        Out << "\n";
      }

      if (B->Term) {
        Out << "    T: ";
        B->Term->print(Out, Ctx);
        Out << "\n";
      }

      Out << "    succs: ";
      if (B->Succs.empty()) {
        Out << "(none)";
      } else {
        bool firstSucc = true;
        for (auto *Succ : B->Succs) {
          if (!firstSucc)
            Out << ", ";
          Out << "B" << Succ->getInd();
          firstSucc = false;
        }
      }
      Out << "\n\n";
    }

    Out << "}\n";
  }

  Out << "===============================\n";
}

void dumpIRProgramJSON(llvm::raw_ostream &Out, IRProgram &P,
                       clang::ASTContext &Context) {
  IRPrintContext Ctx{
      .ASTCtx = Context, .NewlineSymbol = " ", .GraphVizEscapeChars = false};

  // Helper to escape strings for JSON
  auto escapeJSON = [](const std::string &S) -> std::string {
    std::string Result;
    for (char C : S) {
      switch (C) {
      case '"':
        Result += "\\\"";
        break;
      case '\\':
        Result += "\\\\";
        break;
      case '\n':
        Result += "\\n";
        break;
      case '\r':
        Result += "\\r";
        break;
      case '\t':
        Result += "\\t";
        break;
      default:
        Result += C;
        break;
      }
    }
    return Result;
  };

  // Helper to print an IR node to a string
  auto printToString = [&](auto *Node) -> std::string {
    std::string Buf;
    llvm::raw_string_ostream SS(Buf);
    Node->print(SS, Ctx);
    SS.flush();
    return escapeJSON(Buf);
  };

  Out << "{\n  \"functions\": [\n";

  bool firstFunc = true;
  for (auto &FPtr : P) {
    IRFunction *F = FPtr.get();
    if (!firstFunc)
      Out << ",\n";
    firstFunc = false;

    Out << "    {\n";
    Out << "      \"name\": \"" << escapeJSON(F->getName()) << "\",\n";

    // Vars
    Out << "      \"vars\": [\n";
    bool firstVar = true;
    for (auto &V : F->Vars) {
      if (!firstVar)
        Out << ",\n";
      firstVar = false;

      std::string Kind;
      switch (V.DeclLoc) {
      case IRVarDecl::ARG:
        Kind = "arg";
        break;
      case IRVarDecl::LOCAL:
        Kind = "local";
        break;
      default:
        Kind = "var";
        break;
      }
      Out << "        {\"name\": \"" << escapeJSON(GetSym(V.Name))
          << "\", \"kind\": \"" << Kind << "\"}";
    }
    Out << "\n      ],\n";

    // Blocks
    Out << "      \"blocks\": [\n";
    bool firstBlock = true;
    for (auto &BPtr : *F) {
      IRBasicBlock *B = BPtr.get();
      if (!firstBlock)
        Out << ",\n";
      firstBlock = false;

      Out << "        {\n";
      Out << "          \"id\": \"B" << B->getInd() << "\",\n";

      // Preds
      Out << "          \"preds\": [";
      bool firstPred = true;
      B->iteratePreds([&](IRBasicBlock *Pred) {
        if (!firstPred)
          Out << ", ";
        Out << "\"B" << Pred->getInd() << "\"";
        firstPred = false;
      });
      Out << "],\n";

      // Instructions
      Out << "          \"instrs\": [";
      bool firstInstr = true;
      for (auto &S : *B) {
        if (!firstInstr)
          Out << ", ";
        Out << "\"" << printToString(S.get()) << "\"";
        firstInstr = false;
      }
      Out << "],\n";

      // Terminator
      Out << "          \"terminator\": ";
      if (B->Term)
        Out << "\"" << printToString(B->Term) << "\"";
      else
        Out << "null";
      Out << ",\n";

      // Succs
      Out << "          \"succs\": [";
      bool firstSucc = true;
      for (auto *Succ : B->Succs) {
        if (!firstSucc)
          Out << ", ";
        Out << "\"B" << Succ->getInd() << "\"";
        firstSucc = false;
      }
      Out << "]\n";

      Out << "        }";
    }
    Out << "\n      ]\n";

    Out << "    }";
  }

  Out << "\n  ]\n}\n";
}