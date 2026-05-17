#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/StmtVisitor.h>
#include <clang/Analysis/CFG.h>
#include <unordered_map>

#include "IR.hpp"
#include "OpenCilk2IR.hpp"
#include "desugarOpenCilk.hpp"
#include "clang/AST/ASTFwd.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExprCilk.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtCilk.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/Support/ErrorHandling.h"

std::set<std::string> GIgnoreFns;
std::vector<clang::RecordDecl *> GRecordDecls;
//////////////////////////////////
// Scan AST for relevant tasks //
////////////////////////////////
using FunLookupTy =
    std::unordered_map<const clang::FunctionDecl *, IRFunction *>;

// collect variables used inside the while condition
class CondVarCollector : public clang::RecursiveASTVisitor<CondVarCollector> {
public:
  std::set<clang::ValueDecl *> Vars;

  bool VisitDeclRefExpr(clang::DeclRefExpr *DRE) {
    if (auto *VD = llvm::dyn_cast<clang::ValueDecl>(DRE->getDecl())) {
      Vars.insert(VD);
    }
    return true;
  }
};

// Collect all variables that are assigned (written to) inside the while body
// but declared OUTSIDE it. These are the loop-carried variables that must be
// promoted to arguments when converting a while-with-sync to tail recursion.
// Variables declared inside the while body (like loop-local temporaries)
// are excluded since they don't carry state across iterations.
class LoopBodyVarCollector
    : public clang::RecursiveASTVisitor<LoopBodyVarCollector> {
public:
  std::set<clang::ValueDecl *> AssignedVars;
  std::set<clang::ValueDecl *> DeclaredVars;

  bool VisitVarDecl(clang::VarDecl *VD) {
    DeclaredVars.insert(VD);
    return true;
  }

  bool VisitBinaryOperator(clang::BinaryOperator *BO) {
    if (BO->isAssignmentOp()) {
      if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(
              BO->getLHS()->IgnoreParenImpCasts())) {
        if (auto *VD = llvm::dyn_cast<clang::ValueDecl>(DRE->getDecl())) {
          AssignedVars.insert(VD);
        }
      }
    }
    return true;
  }

  bool VisitUnaryOperator(clang::UnaryOperator *UO) {
    if (UO->isIncrementDecrementOp()) {
      if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(
              UO->getSubExpr()->IgnoreParenImpCasts())) {
        if (auto *VD = llvm::dyn_cast<clang::ValueDecl>(DRE->getDecl())) {
          AssignedVars.insert(VD);
        }
      }
    }
    return true;
  }

  // Get the loop-carried variables: assigned in body but not declared in body
  std::set<clang::ValueDecl *> getLoopCarriedVars() {
    std::set<clang::ValueDecl *> Result;
    for (auto *VD : AssignedVars) {
      if (DeclaredVars.find(VD) == DeclaredVars.end()) {
        Result.insert(VD);
      }
    }
    return Result;
  }
};

// Return the definition FunctionDecl if available, otherwise the decl itself.
static clang::FunctionDecl *toDefinition(clang::FunctionDecl *FD) {
  if (auto *Def = FD->getDefinition())
    return Def;
  return FD;
}

class CilkAnalyzeVisitor
    : public clang::RecursiveASTVisitor<CilkAnalyzeVisitor> {
  FunctionDecl *CurrF = nullptr;
  bool WhileCtx = false;
  bool SyncInWhile = false;

public:
  std::set<FunctionDecl *> Tasks;
  std::set<FunctionDecl *> TaskCallers;
  std::unordered_map<FunctionDecl *, std::vector<WhileStmt *>> WhileWithSync;

  explicit CilkAnalyzeVisitor() {}

  bool VisitFunctionDecl(FunctionDecl *Decl) {
    if (GIgnoreFns.find(Decl->getName().str()) != GIgnoreFns.end()) {
      return false;
    }
    CurrF = toDefinition(Decl);
    return true;
  }

  void HandleSpawn(CallExpr *Expr) {
    if (Expr->getDirectCallee()) {
      Tasks.insert(toDefinition(Expr->getDirectCallee()));
      TaskCallers.insert(CurrF);
    } else {
      PANIC("cannot deduce destination of spawn");
    }
  }

  bool VisitCallExpr(CallExpr *Expr) {
    auto *Callee = Expr->getDirectCallee();
    if (Callee && Tasks.find(toDefinition(Callee)) != Tasks.end()) {
      HandleSpawn(Expr);
    }
    return true;
  }

  bool VisitCilkSpawnExpr(CilkSpawnExpr *Expr) {
    if (auto *CExpr = dyn_cast<CallExpr>(Expr->getSpawnedExpr())) {
      HandleSpawn(CExpr);
    } else {
      PANIC("unrecognized spawned expression, should be a function call");
    }
    return true;
  }

  bool VisitCilkSpawnStmt(CilkSpawnStmt *Stmt) {
    if (auto *CExpr = dyn_cast<CallExpr>(Stmt->getSpawnedStmt())) {
      HandleSpawn(CExpr);
    } else {
      PANIC("unrecognized spawned expression, should be a function call");
    }
    return true;
  }

  bool VisitWhileStmt(WhileStmt *WS) {

    bool SavedWhileCtx = WhileCtx;
    bool SavedSyncInWhile = SyncInWhile;
    WhileCtx = true;
    SyncInWhile = false;
    TraverseStmt(WS->getBody());
    if (SyncInWhile) {
      Tasks.insert(CurrF);
      WhileWithSync[CurrF].push_back(WS);
    }

    WhileCtx = SavedWhileCtx;
    SyncInWhile = SavedSyncInWhile;
    return true;
  }

  bool VisitCilkSyncStmt(CilkSyncStmt *Node) {
    if (WhileCtx == true) {
      SyncInWhile = true;
      return true;
    }
    return true;
  }
};

////////////////////////
// Convert AST to IR //
//////////////////////

static BinopIRExpr::BinopOp astOpToIROp(clang::BinaryOperatorKind K) {
  switch (K) {
  case clang::BO_LT:
    return BinopIRExpr::BINOP_LT;
  case clang::BO_GT:
    return BinopIRExpr::BINOP_GT;
  case clang::BO_LE:
    return BinopIRExpr::BINOP_LE;
  case clang::BO_GE:
    return BinopIRExpr::BINOP_GE;
  case clang::BO_EQ:
    return BinopIRExpr::BINOP_EQ;
  case clang::BO_NE:
    return BinopIRExpr::BINOP_NEQ;
  default:
    llvm_unreachable("Unsupported comparison op in desugared condition");
  }
}

class Stmt2IRVisitor : public clang::StmtVisitor<Stmt2IRVisitor> {
private:
  IRBasicBlock *CurrB;
  std::vector<IRExpr *> ExprStack;

  std::unordered_map<ASTVarRef, IRVarRef> &VarLookup;
  std::set<FunctionDecl *> Tasks;
  std::set<ASTVarRef> LoopCarriedVars;
  IRFunction *F;
  bool ExprCtx = false;
  bool CallCtx = false;
  bool SpawnCtx = false;
  bool SyncNext = false;
  bool WhileCtx = false;
  bool SyncInWhile = false;
  // Stack of switch join blocks for break-in-switch handling.
  // A nullptr entry acts as a loop barrier (breaks inside loops don't exit the
  // switch).
  std::vector<IRBasicBlock *> SwitchBreakStack;

  /* Basicially get a Clang expression visit it then in the visit appropriate
  function and convert it to an IR and return it throught the stack ExprStack
  all this while we mark that we are in the context of this expression in
  case of nested calls inside */
  IRExpr *getExpr(Expr *E) {
    assert(E);
    ExprCtx = true;
    Stmt2IRVisitor::Visit(E);
    assert(ExprStack.back());
    auto *IRE = ExprStack.back();
    ExprStack.pop_back();
    ExprCtx = false;
    return IRE;
  }

  /* Come back here; most probably when waiting  for a cilk sync.
  Check when cilk sync is set to true */
  void pushIRStmt(IRStmt *S) {
    CurrB->pushStmtBack(S);
    if (SyncNext) {
      VisitCilkSyncStmt(nullptr);
      SyncNext = false;
    }
  }

  void handleStmt(Stmt *S) {
    assert(S);
    Stmt2IRVisitor::Visit(S);
    if (ExprStack.size() > 0) {
      // should not be expecting to process statements inside expressions
      assert(ExprStack.size() == 1);
      auto *E = ExprStack.back();
      ExprStack.pop_back();
      auto *EW = new ExprWrapIRStmt(E);
      // assert(isa<CallIRExpr>(EW->Expr.get()));
      pushIRStmt((IRStmt *)EW);
    }
  }

  void handleAssign(IRExpr *Dest, IRExpr *Src) {
    if (auto *DI = dyn_cast<IdentIRExpr>(Dest)) {
      auto *CS = new CopyIRStmt(DI->Ident, Src);
      delete DI;
      pushIRStmt((IRStmt *)CS);
    } else if (auto *LDest = dyn_cast<IRLvalExpr>(Dest)) {
      auto *SS = new StoreIRStmt(LDest, Src);
      pushIRStmt((IRStmt *)SS);
    } else {
      llvm_unreachable("Unsupported LHS of assign");
    }
  }

  void handleArrayInitList(IRVarRef VR, QualType VarTy, InitListExpr *ILE) {
    auto *ArrTy = dyn_cast<ConstantArrayType>(VarTy->getAsArrayTypeUnsafe());
    if (!ArrTy) {
      PANIC("unsupported: non-constant array initializer list");
    }

    if (!ILE->isSemanticForm() && ILE->getSemanticForm()) {
      ILE = ILE->getSemanticForm();
    }

    const auto ElemTy = ArrTy->getElementType();
    const uint64_t ElemCount = ArrTy->getSize().getZExtValue();

    for (uint64_t I = 0; I < ElemCount; ++I) {
      Expr *Init = nullptr;
      if (I < ILE->getNumInits()) {
        Init = ILE->getInit(I);
      } else if (ILE->hasArrayFiller()) {
        Init = ILE->getArrayFiller();
      }

      IRExpr *Src =
          Init ? getExpr(Init) : static_cast<IRExpr *>(new IntLiteralIRExpr(0));
      auto *Dest =
          new IndexIRExpr(new IdentIRExpr(VR), new IntLiteralIRExpr(I), ElemTy);
      handleAssign(Dest, Src);
    }
  }

  void handleVarInit(IRVarRef VR, VarDecl *VD) {
    if (!VD->hasInit()) {
      return;
    }

    Expr *Init = VD->getInit()->IgnoreParenImpCasts();

    if (auto *CE = llvm::dyn_cast<clang::CXXConstructExpr>(Init)) {
      if (CE->getNumArgs() == 0)
        return;
      // Preserve constructor-initialization syntax .
      // If the constructor is a real (non-elidable, non-copy) call, skip the
      // CopyIRStmt and let printLocals emit "Type name(args);" from ASTDecl.
      if (!CE->isElidable() && !CE->getConstructor()->isCopyConstructor())
        return;
      if (CE->getNumArgs() == 1)
        Init = CE->getArg(0)->IgnoreParenImpCasts();
    }

    if (auto *ILE = llvm::dyn_cast<InitListExpr>(Init)) {
      if (VD->getType()->isArrayType()) {
        handleArrayInitList(VR, VD->getType(), ILE);
        return;
      }

      if (auto *RD = VD->getType()->getAsRecordDecl()) {
        auto FieldIt = RD->field_begin();
        for (unsigned I = 0; I < ILE->getNumInits(); ++I, ++FieldIt) {
          if (FieldIt == RD->field_end())
            break;
          IRExpr *Src = getExpr(ILE->getInit(I));
          auto *Dest = new AccessIRExpr(VR, FieldIt->getName().str(), false);
          handleAssign(Dest, Src);
        }
        return;
      }
    }

    auto *IE = getExpr(VD->getInit());
    pushIRStmt(new CopyIRStmt(VR, IE));
  }

  // Emit `if (Cond) break;` in CurrB, then advance CurrB to the continuation
  // block.  The BreakIRStmt has no explicit successor — downstream passes
  // resolve it to the enclosing loop's join block.
  void emitIfBreak(IRExpr *Cond) {
    auto *CheckB = CurrB;
    auto *BreakB = F->createBlock();
    auto *ContB = F->createBlock();

    CheckB->Term = new IfIRStmt(Cond);
    CheckB->Succs.insert(BreakB); // true  → break
    CheckB->Succs.insert(ContB);  // false → continue body

    CurrB = BreakB;
    CurrB->Term = new BreakIRStmt();

    CurrB = ContB;
  }

  // Emit the desugared prefix for a loop body that was produced by
  // analyzeCondForDesugar:
  //   1. AssignBO as a statement   (e.g. h = *curr_high)
  //   2. if (!(clean_cond)) break; (e.g. if (!(h > pivot)) break;)
  // Advances CurrB to the block where the original loop body should follow.
  void emitDesugaredBodyPrefix(const WhileCondDesugar &DS) {
    // Emit the assignment (e.g. h = *curr_high) as a regular statement.
    handleStmt(DS.AssignBO);

    // Recover the IR variable that was just assigned.
    auto *AssignedLHS = DS.AssignBO->getLHS()->IgnoreParenImpCasts();
    auto *AssignedDRE = dyn_cast<DeclRefExpr>(AssignedLHS);
    assert(AssignedDRE && "Desugared assignment LHS must be a simple variable");

    IRExpr *AssignedVarExpr;
    auto It = VarLookup.find(AssignedDRE->getDecl());
    if (It != VarLookup.end())
      AssignedVarExpr = new IdentIRExpr(It->second);
    else
      AssignedVarExpr = new ASTLiteralIRExpr(AssignedDRE);

    IRExpr *NegBreakCond;
    if (DS.CompBO) {
      // Clean condition: assigned_var op CompBO->RHS
      IRExpr *RHSExpr = getExpr(DS.CompBO->getRHS());
      auto Op = astOpToIROp(DS.CompBO->getOpcode());
      auto *CleanCond = new BinopIRExpr(Op, AssignedVarExpr, RHSExpr);
      NegBreakCond = new UnopIRExpr(UnopIRExpr::UNOP_L_NOT, CleanCond);
    } else {
      // Condition is just the assignment: break if the assigned value is falsy.
      NegBreakCond = new UnopIRExpr(UnopIRExpr::UNOP_L_NOT, AssignedVarExpr);
    }

    emitIfBreak(NegBreakCond);
  }

public:
  Stmt2IRVisitor(IRFunction *F,
                 std::unordered_map<ASTVarRef, IRVarRef> &VarLookup,
                 std::set<FunctionDecl *> Tasks)
      : F(F), VarLookup(VarLookup), Tasks(Tasks) {
    CurrB = F->createBlock();
  }

  IRBasicBlock *getCurrBlock() { return CurrB; }

  void VisitStmt(Stmt *S) {
    llvm::errs() << "Unhandled statement node: " << S->getStmtClassName()
                 << "\n";
    llvm_unreachable("Unhandled statement node");
  }
  void VisitExpr(Expr *E) {
    llvm::errs() << "Unhandled expr node: " << E->getStmtClassName() << "\n";
    llvm_unreachable("Unhandled expr node");
  }

  void HandleCilkSpawn(IRExpr *SpawnedE) {
    if (auto *CallE = dyn_cast<CallIRExpr>(SpawnedE)) {
      std::vector<IRExpr *> Args;
      for (auto &Arg : CallE->Args) {
        Args.push_back(Arg.get());
        Arg.release();
      }
      IRFunRef FR = CallE->Fn;
      delete CallE;
      auto *SE = new ISpawnIRExpr(FR, Args);
      ExprStack.push_back((IRExpr *)SE);
    } else {
      PANIC("unsupported: cilk spawn on non call expression");
    }
  }

  ////////////
  // Stmts //
  //////////

  void VisitCompoundStmt(CompoundStmt *Node) {
    for (auto &child : Node->children()) {
      handleStmt(child);
    }
  }

  void VisitLabelStmt(LabelStmt *Node) {
    if (Node->getDecl()->getName() == "__bombyx_dae_here") {
      pushIRStmt(new ScopeAnnotIRStmt(ScopeAnnot::SA_DAE_HERE));
      handleStmt(Node->getSubStmt());
    } else {
      PANIC("unsupported: label statement")
    }
  }

  void VisitNullStmt(NullStmt *Node) {}

  void VisitDeclStmt(DeclStmt *Node) {
    for (auto *D : Node->decls()) {
      auto *VD = dyn_cast<VarDecl>(D);
      if (!VD)
        continue; // skip implicit decls like lambda closure types
      Sym VDS = PutSym(VD->getName().str());
      F->Vars.push_back(IRVarDecl{
          .Type = VD->getType(),
          .Name = VDS,
          .DeclLoc = IRVarDecl::LOCAL,
          .ASTDecl = VD,
      });
      IRVarRef VR = &F->Vars.back();
      VarLookup[VD] = VR;
      handleVarInit(VR, VD);
    }
  }

  void VisitIfStmt(IfStmt *IS) {
    // ??
    assert(!IS->hasInitStorage());

    auto *Cond = getExpr(IS->getCond());

    auto *IRS = new IfIRStmt(Cond);
    CurrB->Term = (IRTerminatorStmt *)IRS;
    IRBasicBlock *BranchB = CurrB;
    CurrB = F->createBlock();
    BranchB->Succs.insert(CurrB);
    IRBasicBlock *JoinB = F->createBlock();
    handleStmt(IS->getThen());
    CurrB->Succs.insert(JoinB);

    if (IS->hasElseStorage()) {
      CurrB = F->createBlock();
      BranchB->Succs.insert(CurrB);
      handleStmt(IS->getElse());
      CurrB->Succs.insert(JoinB);
    } else {
      BranchB->Succs.insert(JoinB);
    }
    CurrB = JoinB;
  }

  // drop statements after a return
  void VisitForStmt(ForStmt *FS) {
    WhileCondDesugar DS;
    if (FS->getCond())
      DS = analyzeCondForDesugar(FS->getCond());

    auto *ForB = F->createBlock();
    auto *BodyB = F->createBlock();
    auto *IncB = F->createBlock();
    auto *JoinB = F->createBlock();

    CurrB->Succs.insert(ForB);
    IncB->Succs.insert(ForB);
    ForB->Succs.insert(BodyB);
    ForB->Succs.insert(JoinB);

    CurrB = ForB;
    if (FS->getInit())
      handleStmt(FS->getInit());
    assert(CurrB == ForB);
    IRStmt *InitS = nullptr;
    if (CurrB->begin() != CurrB->end() && CurrB->back()) {
      InitS = CurrB->back().get();
      InitS->setSilent();
    }

    CurrB = IncB;
    if (FS->getInc())
      handleStmt(FS->getInc());
    assert(CurrB == IncB);
    IRStmt *IncS = nullptr;
    if (CurrB->begin() != CurrB->end() && CurrB->back()) {
      IncS = CurrB->back().get();
      IncS->setSilent();
    }

    IRExpr *Cond = nullptr;
    if (DS.AssignBO) {
      // Use only the part of the condition before && as the loop guard;
      // the assignment and inner check move into the body.
      Cond = DS.OuterCond ? getExpr(DS.OuterCond)
                          : static_cast<IRExpr *>(new IntLiteralIRExpr(1));
    } else if (FS->getCond()) {
      Cond = getExpr(FS->getCond());
    } else {
      // for(;;) — no condition means loop forever.
      Cond = new IntLiteralIRExpr(1);
    }

    auto *ForS = new LoopIRStmt(Cond, IncS, InitS);
    ForB->Term = (IRTerminatorStmt *)ForS;

    CurrB = BodyB;

    if (DS.AssignBO)
      emitDesugaredBodyPrefix(
          DS); // advances CurrB to the post-break-check block

    SwitchBreakStack.push_back(
        nullptr); // loop masks any enclosing switch break
    handleStmt(FS->getBody());
    SwitchBreakStack.pop_back();
    CurrB->Succs.insert(IncB);

    CurrB = JoinB;
  }

  void VisitReturnStmt(ReturnStmt *RS) {
    assert(!CurrB->Term);
    IRExpr *RE = nullptr;
    if (RS->getRetValue()) {
      RE = getExpr(RS->getRetValue());
    }
    auto *RetS = new ReturnIRStmt(RE);
    CurrB->Term = (IRTerminatorStmt *)RetS;
  }

  void VisitWhileStmt(WhileStmt *WS) {
    WhileCondDesugar DS = analyzeCondForDesugar(WS->getCond());

    auto *WhileB = F->createBlock();
    auto *BodyB = F->createBlock();
    auto *JoinB = F->createBlock();

    CurrB->Succs.insert(WhileB);
    WhileB->Succs.insert(BodyB);
    WhileB->Succs.insert(JoinB);

    IRExpr *LoopCond;
    if (DS.AssignBO) {
      // Desugared: the outer condition (before &&) guards the loop header;
      // nullptr means while(1) — the assignment+break inside the body exits.
      LoopCond = DS.OuterCond ? getExpr(DS.OuterCond)
                              : static_cast<IRExpr *>(new IntLiteralIRExpr(1));
    } else {
      LoopCond = getExpr(WS->getCond());
    }

    auto *WhileS = new LoopIRStmt(LoopCond, nullptr, nullptr);
    WhileB->Term = (IRTerminatorStmt *)WhileS;

    CurrB = BodyB;

    if (DS.AssignBO)
      emitDesugaredBodyPrefix(
          DS); // advances CurrB to the post-break-check block

    // Save and restore WhileCtx/SyncInWhile so that nested while
    // loops do not clobber the outer loop's state.
    bool SavedWhileCtx = WhileCtx;
    bool SavedSyncInWhile = SyncInWhile;
    WhileCtx = true;
    SyncInWhile = false;
    SwitchBreakStack.push_back(
        nullptr); // loop masks any enclosing switch break
    handleStmt(WS->getBody());
    SwitchBreakStack.pop_back();
    bool ThisSyncInWhile = SyncInWhile;
    WhileCtx = SavedWhileCtx;
    SyncInWhile = SavedSyncInWhile;

    if (ThisSyncInWhile && SavedWhileCtx)
      SyncInWhile = true;

    CurrB->Succs.insert(WhileB);
    CurrB = JoinB;
  }

  void VisitDoWhileStmt(DoStmt *DS) {
    auto *DoAnnot = new ScopeAnnotIRStmt(ScopeAnnot::SA_DO);
    pushIRStmt((IRStmt *)DoAnnot);

    auto *LoopB = F->createBlock();
    auto *JoinB = F->createBlock();

    CurrB->Succs.insert(LoopB);

    CurrB = LoopB;
    handleStmt(DS->getBody());
    CurrB->Succs.insert(LoopB);
    CurrB->Succs.insert(JoinB);

    // probably unneeded?
    // auto *CloseAnnot = new ScopeAnnotIRStmt(ScopeAnnot::SA_CLOSE);
    // CurrB->pushStmtBack((IRStmt*)DoAnnot);

    IRExpr *Cond = getExpr(DS->getCond());
    auto *WhileS = new LoopIRStmt(Cond, nullptr, nullptr);
    CurrB->Term = (IRTerminatorStmt *)WhileS;

    CurrB = JoinB;
  }

  void VisitBreakStmt(BreakStmt *Node) {
    assert(!CurrB->Term);
    if (!SwitchBreakStack.empty() && SwitchBreakStack.back() != nullptr) {
      // Break inside a switch case — jump directly to the switch join block.
      CurrB->Succs.insert(SwitchBreakStack.back());
      CurrB = F->createBlock(); // dead continuation block
    } else {
      CurrB->Term = new BreakIRStmt();
    }
  }

  void VisitSwitchStmt(SwitchStmt *SS) {
    // Emit the entire switch opaquely — the ScopedIRTraverser in downstream
    // passes cannot handle the multi-predecessor join that an if-else
    // desugaring would produce.  The switch body contains no cilk_spawn, so
    // verbatim output is semantically correct. Build a rename map so IR-renamed
    // vars (e.g. size→size2) are printed correctly inside the opaque switch
    // body.
    std::unordered_map<const clang::NamedDecl *, std::string> Renames;
    for (auto &[ASTDecl, IRVar] : VarLookup)
      Renames[ASTDecl] = GetSym(IRVar->Name);
    pushIRStmt(new ASTStmtWrapIRStmt(SS, std::move(Renames)));
  }

  void VisitCilkSyncStmt(CilkSyncStmt *Node) {
    assert(!CurrB->Term);
    if (WhileCtx == true) {
      SyncInWhile = true;
    }
    auto *SyncS = new SyncIRStmt();
    CurrB->Term = (IRTerminatorStmt *)SyncS;
    auto *JoinB = F->createBlock();
    CurrB->Succs.insert(JoinB);
    CurrB = JoinB;
  }

  void VisitCilkSpawnStmt(CilkSpawnStmt *Stmt) {
    Expr *E = dyn_cast<Expr>(Stmt->getSpawnedStmt());
    if (!E) {
      PANIC("unrecognized expression in cilk spawn statement");
    }
    SpawnCtx = true;
    auto *SpawnE = getExpr(E);
    SpawnCtx = false;
    HandleCilkSpawn(SpawnE);
  }

  ////////////
  // Exprs //
  //////////

  void VisitImplicitCastExpr(ImplicitCastExpr *Node) {
    ExprStack.push_back(getExpr(Node->getSubExpr()));
  }

  void VisitParenExpr(ParenExpr *Node) {
    ExprStack.push_back(getExpr(Node->getSubExpr()));
  }

  void VisitMemberExpr(MemberExpr *Node) {
    IRExpr *BaseExpr = getExpr(Node->getBase());
    auto *BaseLval = dyn_cast<IRLvalExpr>(BaseExpr);
    if (!BaseLval) {
      llvm::errs() << "MemberExpr base kind: "
                   << Node->getBase()->getStmtClassName() << "\n";
      llvm::errs() << "MemberExpr text: ";
      Node->printPretty(llvm::errs(), nullptr, PrintingPolicy(LangOptions()));
      llvm::errs() << "\n";
      PANIC("unsupported: non-lvalue base in member expression");
    }
    auto *AE = new AccessIRExpr(
        BaseLval, Node->getMemberDecl()->getName().str(), Node->isArrow());
    ExprStack.push_back(AE);
  }

  void VisitConstantExpr(ConstantExpr *Node) { Visit(Node->getSubExpr()); }

  void VisitIntegerLiteral(IntegerLiteral *Node) {
    auto *LE = new ASTLiteralIRExpr(Node);
    ExprStack.push_back((IRExpr *)LE);
  }

  void VisitFloatingLiteral(FloatingLiteral *Node) {
    auto *LE = new ASTLiteralIRExpr(Node);
    ExprStack.push_back((IRExpr *)LE);
  }

  void VisitStringLiteral(StringLiteral *Node) {
    auto *LE = new ASTLiteralIRExpr(Node);
    ExprStack.push_back((IRExpr *)LE);
  }

  void VisitInitListExpr(InitListExpr *Node) {
    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitCXXConstructExpr(clang::CXXConstructExpr *Node) {
    if (Node->getNumArgs() == 0) {
      ExprStack.push_back(new ASTLiteralIRExpr(Node));
      return;
    }

    if (Node->isElidable() && Node->getNumArgs() == 1) {
      ExprStack.push_back(getExpr(Node->getArg(0)));
      return;
    }

    if (Node->getConstructor()->isCopyConstructor() &&
        Node->getNumArgs() == 1) {
      ExprStack.push_back(getExpr(Node->getArg(0)));
      return;
    }

    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitBinaryOperator(BinaryOperator *Node) {
    if (Node->getOpcode() == clang::BO_Comma) {
      if (!ExprCtx) {
        // for (i=0, j=0; ...) or for (...; i++, j++)
        // emit each operand as its own statement in order.
        handleStmt(Node->getLHS());
        handleStmt(Node->getRHS());
      } else {
        // x = (a++, b+1)
        // LHS is a side-effect statement, RHS is the value.
        ExprCtx = false;
        handleStmt(Node->getLHS());
        ExprCtx = true;
        ExprStack.push_back(getExpr(Node->getRHS()));
      }
      return;
    }

    IRExpr *Left = getExpr(Node->getLHS());
    IRExpr *Right = getExpr(Node->getRHS());

    BinopIRExpr::BinopOp Op;
    switch (Node->getOpcode()) {
    case clang::BO_Assign:
      break;
    case clang::BO_MulAssign:
    case clang::BO_Mul:
      Op = BinopIRExpr::BINOP_MUL;
      break;
    case clang::BO_DivAssign:
    case clang::BO_Div:
      Op = BinopIRExpr::BINOP_DIV;
      break;
    case clang::BO_RemAssign:
    case clang::BO_Rem:
      Op = BinopIRExpr::BINOP_MOD;
      break;
    case clang::BO_AddAssign:
    case clang::BO_Add:
      Op = BinopIRExpr::BINOP_ADD;
      break;
    case clang::BO_SubAssign:
    case clang::BO_Sub:
      Op = BinopIRExpr::BINOP_SUB;
      break;
    case clang::BO_ShlAssign:
    case clang::BO_Shl:
      Op = BinopIRExpr::BINOP_SHL;
      break;
    case clang::BO_ShrAssign:
    case clang::BO_Shr:
      Op = BinopIRExpr::BINOP_SHR;
      break;
    case clang::BO_LT:
      Op = BinopIRExpr::BINOP_LT;
      break;
    case clang::BO_GT:
      Op = BinopIRExpr::BINOP_GT;
      break;
    case clang::BO_LE:
      Op = BinopIRExpr::BINOP_LE;
      break;
    case clang::BO_GE:
      Op = BinopIRExpr::BINOP_GE;
      break;
    case clang::BO_EQ:
      Op = BinopIRExpr::BINOP_EQ;
      break;
    case clang::BO_NE:
      Op = BinopIRExpr::BINOP_NEQ;
      break;
    case clang::BO_AndAssign:
    case clang::BO_And:
      Op = BinopIRExpr::BINOP_AND;
      break;
    case clang::BO_OrAssign:
    case clang::BO_Or:
      Op = BinopIRExpr::BINOP_OR;
      break;
    case clang::BO_XorAssign:
    case clang::BO_Xor:
      Op = BinopIRExpr::BINOP_XOR;
      break;
    case clang::BO_LAnd:
      Op = BinopIRExpr::BINOP_LAND;
      break;
    case clang::BO_LOr:
      Op = BinopIRExpr::BINOP_LOR;
      break;
    default: {
      llvm::errs() << "Unknown binary operator: " << Node->getOpcodeStr()
                   << "\n";
      llvm_unreachable("Unknown binary operator");
    }
    }

    if (Node->getOpcode() == clang::BO_Assign) {
      assert(!ExprCtx);
      if (dyn_cast<ASTLiteralIRExpr>(Left)) {
        // Global or untracked variable — emit the whole assignment opaquely.
        delete Left;
        delete Right;
        pushIRStmt(new ExprWrapIRStmt(new ASTLiteralIRExpr(Node)));
      } else {
        handleAssign(Left, Right);
      }
    } else if (Node->getOpcode() > clang::BO_Assign &&
               Node->getOpcode() <= clang::BO_OrAssign) {
      assert(!ExprCtx);
      auto *DI = dyn_cast<IdentIRExpr>(Left);
      if (!DI) {
        // compound assignment opaquely so VarRenamePrinterHelper
        delete Left;
        delete Right;
        pushIRStmt(new ExprWrapIRStmt(new ASTLiteralIRExpr(Node)));
      } else {
        auto *LeftC = new IdentIRExpr(DI->Ident);
        BinopIRExpr *BE = new BinopIRExpr(Op, LeftC, Right);
        handleAssign(Left, (IRExpr *)BE);
      }
    } else {
      BinopIRExpr *BE = new BinopIRExpr(Op, Left, Right);
      ExprStack.push_back((IRExpr *)BE);
    }
  }

  void VisitUnaryOperator(UnaryOperator *Node) {
    IRExpr *SE = getExpr(Node->getSubExpr());

    UnopIRExpr::UnopOp Op;
    switch (Node->getOpcode()) {
    case clang::UO_AddrOf: {
      auto *RE = new RefIRExpr(SE);
      ExprStack.push_back((IRExpr *)RE);
      return;
    }
    case clang::UO_Deref: {
      QualType PointeeType = (Node->getSubExpr()->getType())->getPointeeType();
      auto *DR = new DRefIRExpr(SE, PointeeType);
      ExprStack.push_back((IRExpr *)DR);
      return;
    }
    case clang::UO_LNot:
      Op = UnopIRExpr::UNOP_L_NOT;
      break;
    case clang::UO_Not:
      Op = UnopIRExpr::UNOP_NOT;
      break;
    case clang::UO_Minus:
      Op = UnopIRExpr::UNOP_NEG;
      break;
    case clang::UO_PreDec:
      Op = UnopIRExpr::UNOP_PREDEC;
      break;
    case clang::UO_PostDec:
      Op = UnopIRExpr::UNOP_POSTDEC;
      break;
    case clang::UO_PreInc:
      Op = UnopIRExpr::UNOP_PREINC;
      break;
    case clang::UO_PostInc:
      Op = UnopIRExpr::UNOP_POSTINC;
      break;
    default: {
      llvm::errs() << "Unknown unary operator: "
                   << Node->getOpcodeStr(Node->getOpcode()) << "\n";
      llvm_unreachable("Unknown unary operator");
    }
    }

    UnopIRExpr *UE = new UnopIRExpr(Op, SE);
    ExprStack.push_back((IRExpr *)UE);
  }

  void VisitArraySubscriptExpr(ArraySubscriptExpr *Node) {
    auto *Arr = getExpr(Node->getBase());

    if (auto *ArrLval = dyn_cast<IRLvalExpr>(Arr)) {
      auto *Ind = getExpr(Node->getIdx());
      IRType ArrType = (Node->getBase()->getType())->getPointeeType();
      IndexIRExpr *IE = new IndexIRExpr(ArrLval, Ind, ArrType);
      ExprStack.push_back((IRExpr *)IE);
    } else {
      // Emit the whole subscript expression opaquely
      // VarRenamePrinterHelper will rename any tracked variables inside.
      delete Arr;
      ExprStack.push_back(new ASTLiteralIRExpr(Node));
    }
  }

  void VisitCallExpr(CallExpr *Node) {
    // Calls to non-identifier-named functions (operators, destructors, etc.)
    // cannot be represented in the IR and are emitted opaquely.
    auto *DirectCallee = Node->getDirectCallee();
    if (DirectCallee && !DirectCallee->getDeclName().isIdentifier()) {
      ExprStack.push_back(new ASTLiteralIRExpr(Node));
      return;
    }

    CallCtx = true;
    auto *FnExpr = getExpr(Node->getCallee());
    CallCtx = false;

    std::vector<IRExpr *> Args;
    for (auto *ArgE : Node->arguments()) {
      Args.push_back(getExpr(ArgE));
    }

    if (auto *FnIdentE = dyn_cast<FIdentIRExpr>(FnExpr)) {
      IRFunRef FnIdent = FnIdentE->FR;
      delete FnIdentE;
      CallIRExpr *CE = new CallIRExpr(FnIdent, Args);
      ExprStack.push_back((IRExpr *)CE);
    } else {
      llvm::errs()
          << "Unsupported: non function identifier used for call expression.";
      llvm_unreachable("Non function identifier used for call expression.");
    }

    // the first function has cilk_spawn but the second doesn't
    // (implicit cilk spawn, so we are adding a sync)
    if (!SpawnCtx && Node->getDirectCallee() &&
        Tasks.find(toDefinition(Node->getDirectCallee())) != Tasks.end()) {
      auto *SpawnedE = ExprStack.back();
      ExprStack.pop_back();
      HandleCilkSpawn(SpawnedE);
      SyncNext = true;
    }
  }

  void VisitConditionalOperator(ConditionalOperator *Node) {
    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *Node) {
    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitLambdaExpr(LambdaExpr *Node) {
    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitCXXNewExpr(CXXNewExpr *Node) {
    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitCXXDeleteExpr(CXXDeleteExpr *Node) {
    ExprStack.push_back(new ASTLiteralIRExpr(Node));
  }

  void VisitExprWithCleanups(ExprWithCleanups *Node) {
    ExprStack.push_back(getExpr(Node->getSubExpr()));
  }

  void VisitMaterializeTemporaryExpr(MaterializeTemporaryExpr *Node) {
    ExprStack.push_back(getExpr(Node->getSubExpr()));
  }

  void VisitCStyleCastExpr(CStyleCastExpr *Node) {
    auto *E = getExpr(Node->getSubExpr());
    auto *CastE = new CastIRExpr(Node->getType(), E);
    ExprStack.push_back(CastE);
  }

  void VisitCilkSpawnExpr(CilkSpawnExpr *Node) {
    SpawnCtx = true;
    auto *SpawnE = getExpr(Node->getSpawnedExpr());
    SpawnCtx = false;
    HandleCilkSpawn(SpawnE);
  }

  void VisitDeclRefExpr(DeclRefExpr *DRE) {
    ASTVarRef VR = DRE->getDecl();
    // printf("looking for %p\n", VR);
    // for (auto it = VarLookup.begin(); it != VarLookup.end(); it++) {
    //   auto &[k, v] = *it;
    //   printf("%p -> %s\n", k, v->Name.c_str());
    // }
    if (VarLookup.find(VR) != VarLookup.end()) {
      auto *IS = new IdentIRExpr(VarLookup[VR]);
      ExprStack.push_back((IRExpr *)IS);
    } else if (CallCtx) {
      IRFunRef FR(VR);
      auto *FS = new FIdentIRExpr(FR);
      ExprStack.push_back((IRExpr *)FS);
    } else {
      // TODO put a warning here instead
      // llvm::errs() << "Could not find variable: " << VR->getDeclName();
      // abort();
      ExprStack.push_back(new ASTLiteralIRExpr(DRE));
    }
  }
};

// Collects direct call targets in a function body that are not
// Tasks/TaskCallers and have a definition — these are candidates for inlining
// into task IR.
class InlinableCollector
    : public clang::RecursiveASTVisitor<InlinableCollector> {
  std::set<FunctionDecl *> const &Tasks;
  std::set<FunctionDecl *> const &TaskCallers;

public:
  std::vector<FunctionDecl *> Found;

  InlinableCollector(std::set<FunctionDecl *> const &Tasks,
                     std::set<FunctionDecl *> const &TaskCallers)
      : Tasks(Tasks), TaskCallers(TaskCallers) {}

  bool VisitCallExpr(CallExpr *CE) {
    auto *Callee = CE->getDirectCallee();
    if (!Callee || !Callee->getBody())
      return true;
    if (!Callee->getDeclName().isIdentifier())
      return true;
    Callee = toDefinition(Callee);
    if (Tasks.count(Callee) || TaskCallers.count(Callee))
      return true;
    if (GIgnoreFns.count(Callee->getName().str()))
      return true;
    Found.push_back(Callee);
    return true;
  }
};

class Cilk2IRVisitor : public clang::RecursiveASTVisitor<Cilk2IRVisitor> {
private:
  clang::ASTContext *Context;
  IRProgram &P;
  std::set<FunctionDecl *> &Tasks;
  std::set<FunctionDecl *> &TaskCallers;
  std::unordered_map<FunctionDecl *, std::vector<WhileStmt *>> &WhileWithSync;
  std::set<FunctionDecl *> &InlinableFns;

public:
  FunLookupTy FunLookup;
  explicit Cilk2IRVisitor(
      clang::ASTContext *Context, IRProgram &P, std::set<FunctionDecl *> &Tasks,
      std::set<FunctionDecl *> &TaskCallers,
      std::unordered_map<FunctionDecl *, std::vector<WhileStmt *>>
          &WhileWithSync,
      std::set<FunctionDecl *> &InlinableFns)
      : Context(Context), P(P), Tasks(Tasks), TaskCallers(TaskCallers),
        WhileWithSync(WhileWithSync), InlinableFns(InlinableFns) {}

  bool VisitFunctionDecl(clang::FunctionDecl *Decl) {
    if (Tasks.find(Decl) == Tasks.end() &&
        TaskCallers.find(Decl) == TaskCallers.end() &&
        InlinableFns.find(Decl) == InlinableFns.end()) {
      return true;
    }
    // Only process the actual definition, not a forward declaration.
    if (Decl->doesThisDeclarationHaveABody()) {
      IRFunction *F =
          P.createFunc(Decl->getName().str(), Decl->getDeclaredReturnType());
      F->Info.RootFun = Decl;
      std::unordered_map<ASTVarRef, IRVarRef> VarLookup;
      for (auto *Param : Decl->parameters()) {
        Sym PSym = PutSym(Param->getName().str());
        F->Vars.push_back(IRVarDecl{
            .Type = Param->getType(),
            .Name = PSym,
            .DeclLoc = IRVarDecl::ARG,
            .ASTDecl = Param,
        });
        VarLookup[Param] = &F->Vars.back();
      }
      Stmt2IRVisitor sv(F, VarLookup, Tasks);
      sv.Visit(Decl->getBody());

      // Register all redeclarations (forward decls + definition) so that
      // FinalizeVisitor can find the IRFunction regardless of which decl
      // pointer appears at a call/spawn site.
      for (auto *RD : Decl->redecls())
        FunLookup[RD] = F;
    } else if (Tasks.find(Decl) != Tasks.end() && !Decl->getBody()) {
      PANIC("unsupported: forward declaration of task function with no "
            "definition");
    }
    return true;
  }

  bool VisitRecordDecl(clang::RecordDecl *RD) {
    if (RD->isThisDeclarationADefinition()) {
      GRecordDecls.push_back(RD);
    }
    return true;
  }
};

class FinalizeVisitor : public IRExprVisitor<FinalizeVisitor> {
  FunLookupTy &FunLookup;

public:
  FinalizeVisitor(FunLookupTy &FunLookup) : FunLookup(FunLookup) {}

  void VisitISpawn(ISpawnIRExpr *Node) {
    if (auto *VarRef = std::get_if<ASTVarRef>(&Node->Fn)) {
      auto *FR = dyn_cast<FunctionDecl>(*VarRef);
      assert(FR);
      if (FunLookup.find(FR) != FunLookup.end()) {
        auto *SpawnDest = FunLookup[FR];
        Node->Fn = SpawnDest;
        SpawnDest->Info.IsTask = true;
      }
    } else {
      llvm_unreachable("Expected an AST variable reference in ISpawnIRExpr");
    }
  }

  void VisitCall(CallIRExpr *Node) {
    if (auto *VarRef = std::get_if<ASTVarRef>(&Node->Fn)) {
      if (auto *FR = dyn_cast<FunctionDecl>(*VarRef)) {
        auto It = FunLookup.find(FR);
        if (It != FunLookup.end())
          Node->Fn = It->second;
      }
    }
    for (auto &Arg : Node->Args)
      Visit(Arg.get());
  }
};

void finalizeFunction(IRFunction *F, FunLookupTy &FunLookup) {
  FinalizeVisitor FV(FunLookup);
  for (auto &B : *F) {
    for (auto &S : *B.get()) {
      FV.VisitStmt(S.get());
    }
    if (B->Term && isa<ReturnIRStmt>(B->Term)) {
      B->Succs.clear();
      //  B->Succs.insert(F->Exit);
    }
  }
}

void OpenCilk2IR(IRProgram &P, clang::ASTContext *Context, SourceManager &SM) {
  CilkAnalyzeVisitor AVisitor;

  // Pass 1: identify Tasks and TaskCallers
  auto Decls = Context->getTranslationUnitDecl()->decls();
  for (auto &Decl : Decls) {
    if (!SM.isInMainFile(Decl->getLocation()))
      continue;
    AVisitor.TraverseDecl(Decl);
  }

  DBG {
    llvm::outs() << "tasks\n";
    for (auto *Task : AVisitor.Tasks) {
      llvm::outs() << Task->getName() << "\n";
    }
    llvm::outs() << "taskcallers\n";
    for (auto *Task : AVisitor.TaskCallers) {
      llvm::outs() << Task->getName() << "\n";
    }
  }

  // Pass 2: transitively collect inlinable functions — functions called
  // (not spawned) from within Tasks/TaskCallers that need to appear in the IR.
  std::set<FunctionDecl *> InlinableFns;
  {
    std::vector<FunctionDecl *> Worklist;
    std::set<FunctionDecl *> Seen;
    auto enqueue = [&](FunctionDecl *FD) {
      if (Seen.insert(FD).second)
        Worklist.push_back(FD);
    };
    for (auto *F : AVisitor.Tasks)
      enqueue(F);
    for (auto *F : AVisitor.TaskCallers)
      enqueue(F);

    while (!Worklist.empty()) {
      FunctionDecl *FD = Worklist.back();
      Worklist.pop_back();
      if (!FD->getBody())
        continue;
      InlinableCollector IC(AVisitor.Tasks, AVisitor.TaskCallers);
      IC.TraverseDecl(FD);
      for (auto *Callee : IC.Found) {
        if (InlinableFns.insert(Callee).second)
          enqueue(Callee);
      }
    }
  }

  DBG {
    llvm::outs() << "inlinables\n";
    for (auto *F : InlinableFns)
      llvm::outs() << F->getName() << "\n";
  }

  // reserve all real local variable names so that PutSym's never collides with
  // an existing name
  {
    struct VarNameCollector : clang::RecursiveASTVisitor<VarNameCollector> {
      bool VisitVarDecl(clang::VarDecl *VD) {
        ReserveName(VD->getName().str());
        return true;
      }
    } VNC;
    for (auto *FD : AVisitor.Tasks) {
      for (auto *P : FD->parameters())
        ReserveName(P->getName().str());
      if (FD->getBody())
        VNC.TraverseStmt(FD->getBody());
    }
    for (auto *FD : AVisitor.TaskCallers) {
      for (auto *P : FD->parameters())
        ReserveName(P->getName().str());
      if (FD->getBody())
        VNC.TraverseStmt(FD->getBody());
    }
    for (auto *FD : InlinableFns) {
      for (auto *P : FD->parameters())
        ReserveName(P->getName().str());
      if (FD->getBody())
        VNC.TraverseStmt(FD->getBody());
    }
  }

  // Pass 3: convert Tasks, TaskCallers, and InlinableFns to IR
  Cilk2IRVisitor Visitor(Context, P, AVisitor.Tasks, AVisitor.TaskCallers,
                         AVisitor.WhileWithSync, InlinableFns);
  for (auto &Decl : Decls) {
    if (!SM.isInMainFile(Decl->getLocation()))
      continue;
    Visitor.TraverseDecl(Decl);
  }

  for (auto &F : P) {
    finalizeFunction(F.get(), Visitor.FunLookup);
  }
}
