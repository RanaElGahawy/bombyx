#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/TokenKinds.h"
#include "llvm/Support/raw_ostream.h"
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/Tooling.h>
#include <cstdlib>
#include <filesystem>
#include <getopt.h>
#include <optional>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>

#include "core/Cilk1EmuTarget.hpp"
#include "core/TBBTarget.hpp"
#include "core/CountSpawns.hpp"
#include "core/DAE.hpp"
#include "core/FlattenIR.hpp"
#include "hardcilk/HardCilkAnalysis.hpp"
#include "hardcilk/HardCilkDescGen.hpp"
#include "core/IR.hpp"
#include "vitis/VitisHLSTarget.hpp"
#include "vitis/VitisHLSTclGen.hpp"
#include "core/MakeExplicit.hpp"
#include "core/OpenCilk2IR.hpp"
#include "core/OverlapMemAnalysis.hpp"
#include "core/PruneDeadSpawnArgs.hpp"
#include "core/util.hpp"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;
using namespace clang::driver;

int VERBOSITY = 0;

struct ConvertOpts {
  std::set<int> DumpPasses;

  // flags to pass when running the program
  // default: TC_CILK1EMU
  enum { TG_CILK1EMU, TG_HARDCILK, TG_TBB } Target = TG_CILK1EMU;
  bool HCGenDriver = false;
  std::string OutputDir; // -d flag: override base output directory
  std::string AppName;   // stem of input file
};

ConvertOpts GOpts;

class CilkConvert : public clang::ASTConsumer {
private:
  clang::CompilerInstance &CI;

  IRProgram P;
  StringRef OutFilename;

public:
  explicit CilkConvert(clang::CompilerInstance &CI, StringRef OutFilename)
      : CI(CI), OutFilename(OutFilename) {}

  using PassFn = std::function<void(IRProgram &)>;
  void HandleTranslationUnit(clang::ASTContext &Context) {
    std::error_code EC;
    auto &SM = CI.getSourceManager();

    if (CI.getDiagnostics().getNumErrors() != 0) {
      exit(EXIT_FAILURE);
    }

    DriverCallersTy DriverCallers;
    std::optional<HardCilkAnalysisResult> HCAnalysis;

    std::vector<PassFn> Passes{
        [&](IRProgram &P) -> void {
          OpenCilk2IR(P, &Context, SM, DriverCallers);
          // printFullIRProgram(llvm::outs(), P, Context);
        },
        // FlattenIR -> OverlapMemAnalysis -> FlattenIR.
        //
        // The order matters and the double run is deliberate. The first
        // FlattenIR splits every loop containing a *real* spawn/sync into
        // root/reentry/exit tasks and sets IsOverlap/IsOverlapReentry, which is
        // the scope OverlapMemAnalysis needs before it can apply.
        // OverlapMemAnalysis then injects memory syncs, both there and in loops
        // that FlattenIR left intact because they held no spawn at all
        // (countIntersections). The second FlattenIR splits those newly
        // sync-carrying loops; without it restructureLoopsWithSync
        // (FlattenIR.cpp:267) never fires for them.
        //
        // Running FlattenIR twice is safe: its `static int LoopCounter` /
        // NextOverlapId keep numbering monotone, and an already-split loop
        // presents nothing left to split.
        [&](IRProgram &P) -> void { FlattenIR(P); },
        [&](IRProgram &P) -> void { OverlapMemAnalysis(P, Context); },
        [&](IRProgram &P) -> void {
          FlattenIR(P);
          // printFullIRProgram(llvm::outs(), P, Context);
          // dumpIRProgramJSON(llvm::outs(), P, Context);
        },
        [&](IRProgram &P) -> void { DAE(P); },
        [&](IRProgram &P) -> void {
          MakeExplicit(P);
          // dumpIRProgramJSON(llvm::outs(), P, Context);
        },
        [&](IRProgram &P) -> void {
          CountSpawns(P, Context);
          // dumpIRProgramJSON(llvm::outs(), P, Context);
        },
        [&](IRProgram &P) -> void {
          if (GOpts.Target == ConvertOpts::TG_HARDCILK)
            PruneDeadSpawnArgs(P);
        },
        // HardCilk task analysis pass — backend-agnostic, runs before any
        // HardCilk printer.
        [&](IRProgram &P) -> void {
          if (GOpts.Target == ConvertOpts::TG_HARDCILK)
            HCAnalysis = RunHardCilkAnalysis(P);
        },
        [&](IRProgram &P) -> void {
          switch (GOpts.Target) {
          case ConvertOpts::TG_CILK1EMU: {
            llvm::raw_fd_ostream Cilk1Out(OutFilename, EC,
                                          llvm::sys::fs::OF_Text);
            PrintCilk1Emu(P, Cilk1Out, Context, CI);
            std::filesystem::path OutDir =
                std::filesystem::path(OutFilename.str()).parent_path();
            if (OutDir.empty())
              OutDir = ".";

            std::filesystem::path Src =
                std::filesystem::path(BOMBYX_SUPPORT_DIR) / "cilk_explicit.hh";
            std::filesystem::path Dest = OutDir / "cilk_explicit.hh";

            std::error_code CopyEC;
            std::filesystem::copy_file(
                Src, Dest, std::filesystem::copy_options::overwrite_existing,
                CopyEC);
            if (CopyEC) {
              llvm::errs() << "warning: could not copy cilk_explicit.hh to "
                           << Dest.string() << ": " << CopyEC.message() << "\n";
            }
            break;
          };
          case ConvertOpts::TG_TBB: {
            llvm::raw_fd_ostream TBBOut(OutFilename, EC,
                                        llvm::sys::fs::OF_Text);
            PrintTBB(P, TBBOut, Context, CI);
            std::filesystem::path OutDir =
                std::filesystem::path(OutFilename.str()).parent_path();
            if (OutDir.empty())
              OutDir = ".";

            std::filesystem::path Src =
                std::filesystem::path(BOMBYX_SUPPORT_DIR) / "tbb_explicit.hh";
            std::filesystem::path Dest = OutDir / "tbb_explicit.hh";

            std::error_code CopyEC;
            std::filesystem::copy_file(
                Src, Dest, std::filesystem::copy_options::overwrite_existing,
                CopyEC);
            if (CopyEC) {
              llvm::errs() << "warning: could not copy tbb_explicit.hh to "
                           << Dest.string() << ": " << CopyEC.message()
                           << "\n";
            }

            std::string CMakePath = (OutDir / "CMakeLists.txt").string();
            llvm::raw_fd_ostream CMakeOut(CMakePath, EC,
                                          llvm::sys::fs::OF_Text);
            PrintTBBCMake(GOpts.AppName + "_tbb", CMakeOut);
            break;
          };
          case ConvertOpts::TG_HARDCILK: {
            std::filesystem::path OutPath(OutFilename.str());
            std::filesystem::create_directories(OutPath);
            std::string AppName = GOpts.AppName;
            // printFullIRProgram(llvm::errs(), P, Context);
            VitisHLSTarget HT(P, AppName, *HCAnalysis,
                              std::move(DriverCallers));

            // Collect #include directives for the HLS output.
            // For every externally-declared function called in the IR,
            // trace its NamedDecl location up the include chain to the
            // top-level header (e.g. <cstdlib> for std::abs).
            {
              auto &SMSrc = CI.getSourceManager();
              std::vector<std::string> Includes;
              std::set<std::string> Seen;

              auto addInclude = [&](std::string Inc) {
                if (Seen.insert(Inc).second)
                  Includes.push_back(std::move(Inc));
              };

              // For every externally-called function, find its
              // immediate non-internal declaring header.  Internal C++
              // implementation headers (bits/, ext/, detail/) are skipped and
              // we walk up the include chain to the first public header.
              auto findHeaderForDecl =
                  [&](const clang::NamedDecl *D) -> std::string {
                clang::SourceLocation Loc = D->getLocation();
                if (!Loc.isValid() || SMSrc.isInMainFile(Loc))
                  return "";
                bool IsSys = SMSrc.isInSystemHeader(Loc);
                clang::FileID FID = SMSrc.getFileID(Loc);
                while (FID.isValid()) {
                  auto *FE = SMSrc.getFileEntryForID(FID);
                  if (FE) {
                    llvm::StringRef Full = FE->getName();
                    // Skip internal C++ implementation headers.
                    bool IsInternal = Full.contains("/bits/") ||
                                      Full.contains("/ext/") ||
                                      Full.contains("/detail/") ||
                                      Full.contains("/details/");
                    if (!IsInternal) {
                      size_t Slash = Full.rfind('/');
                      std::string Base = (Slash != std::string::npos)
                                             ? Full.substr(Slash + 1).str()
                                             : Full.str();
                      return IsSys ? "#include <" + Base + ">"
                                   : "#include \"" + Base + "\"";
                    }
                  }
                  // Walk up one level in the include chain.
                  clang::SourceLocation IncLoc = SMSrc.getIncludeLoc(FID);
                  if (!IncLoc.isValid())
                    break;
                  FID = SMSrc.getFileID(IncLoc);
                }
                return "";
              };

              // Walk all IR expressions to find ASTVarRef call targets.
              auto walkExpr = [&](auto &&self, IRExpr *E) -> void {
                if (!E)
                  return;
                if (auto *CE = dyn_cast<CallIRExpr>(E)) {
                  if (auto *AV = std::get_if<ASTVarRef>(&CE->Fn)) {
                    std::string H = findHeaderForDecl(*AV);
                    if (!H.empty())
                      addInclude(std::move(H));
                  }
                  for (auto &Arg : CE->Args)
                    self(self, Arg.get());
                  return;
                }
                if (auto *BE = dyn_cast<BinopIRExpr>(E)) {
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
              auto walkStmt = [&](IRStmt *S) {
                if (auto *CS = dyn_cast<CopyIRStmt>(S))
                  walkExpr(walkExpr, CS->Src.get());
                else if (auto *EW = dyn_cast<ExprWrapIRStmt>(S))
                  walkExpr(walkExpr, EW->Expr.get());
                else if (auto *SS = dyn_cast<StoreIRStmt>(S)) {
                  walkExpr(walkExpr, SS->Dest.get());
                  walkExpr(walkExpr, SS->Src.get());
                } else if (auto *IS = dyn_cast<IfIRStmt>(S))
                  walkExpr(walkExpr, IS->Cond.get());
                else if (auto *LS = dyn_cast<LoopIRStmt>(S))
                  walkExpr(walkExpr, LS->Cond.get());
                else if (auto *RS = dyn_cast<ReturnIRStmt>(S))
                  walkExpr(walkExpr, RS->RetVal.get());
                else if (auto *ES = dyn_cast<ESpawnIRStmt>(S))
                  for (auto &Arg : ES->Args)
                    walkExpr(walkExpr, Arg.get());
              };
              for (auto &FPtr : P)
                for (auto &B : *FPtr) {
                  for (auto &S : *B)
                    walkStmt(S.get());
                  if (B->Term)
                    walkStmt(B->Term);
                }

              HT.SetExtraIncludes(std::move(Includes));
            }

            std::string DescJsonName =
                OutFilename.str() + "/" + AppName + ".json";
            llvm::raw_fd_ostream DescJson(DescJsonName, EC,
                                          llvm::sys::fs::OF_Text);
            PrintHardCilkDescJson(AppName, HCAnalysis->TaskInfos,
                                  OutFilename.str(), DescJson);

            std::string HLSCodeName =
                OutFilename.str() + "/" + AppName + ".cpp";
            llvm::raw_fd_ostream HLSCode(HLSCodeName, EC,
                                         llvm::sys::fs::OF_Text);
            HT.PrintHardCilk(HLSCode, Context);

            std::string DefsName =
                OutFilename.str() + "/" + AppName + "_defs.h";
            llvm::raw_fd_ostream Defs(DefsName, EC, llvm::sys::fs::OF_Text);
            HT.PrintDefs(Defs);

            std::string DriverHName =
                OutFilename.str() + "/" + AppName + "Driver.h";
            llvm::raw_fd_ostream DriverH(DriverHName, EC,
                                         llvm::sys::fs::OF_Text);
            HT.PrintDriverHeader(DriverH, Context);

            if (GOpts.HCGenDriver) {
              std::string DriverName =
                  OutFilename.str() + "/" + AppName + "_driver.cpp";
              llvm::raw_fd_ostream Driver(DefsName, EC, llvm::sys::fs::OF_Text);
              HT.PrintDriver(Driver);
            }

            // Generate per-PE Vitis HLS TCL scripts and master build_hls.sh.
            PrintVitisHLSArtifacts(AppName, HCAnalysis->TaskInfos,
                                   "ALVEO_U55C", 300,
                                   OutFilename.str());
            break;
          }
          }
        },
    };

    IRPrintContext Ctx =
        IRPrintContext{.ASTCtx = Context, .NewlineSymbol = "\n"};

    for (int i = 0; i < Passes.size(); i++) {
      Passes[i](P);
      if (GOpts.DumpPasses.find(i) != GOpts.DumpPasses.end()) {
        std::string fname = "ir" + std::to_string(i) + ".dot";
        llvm::raw_fd_ostream DotFile(fname, EC, llvm::sys::fs::OF_Text);
        if (EC) {
          PANIC("could not open file %s", fname.c_str());
        }
        P.dumpGraph(DotFile, Context);
        DotFile.close();
      }
    }
  }
};

class BombyxPragmaHandler : public clang::PragmaHandler {
private:
  void daePragma(clang::Preprocessor &PP, clang::PragmaIntroducer Introducer,
                 clang::Token &FirstToken) {

    clang::Token Tok;
    // Consume remaining tokens until end of directive
    PP.Lex(Tok);
    while (!Tok.is(clang::tok::eod)) {
      PP.Lex(Tok);
    }

    // 1. Create the tokens
    Token LabelTok;
    LabelTok.startToken();
    LabelTok.setKind(tok::identifier);
    LabelTok.setIdentifierInfo(PP.getIdentifierInfo("__bombyx_dae_here"));

    Token ColTok;
    ColTok.startToken();
    ColTok.setKind(tok::colon);

    Token SemiTok;
    SemiTok.startToken();
    SemiTok.setKind(tok::semi);

    SmallVector<Token, 3> TokenList;
    TokenList.push_back(LabelTok);
    TokenList.push_back(ColTok);
    TokenList.push_back(SemiTok);

    for (Token &Tok : TokenList)
      Tok.setLocation(FirstToken.getLocation());

    ArrayRef TokenArray = TokenList;
    PP.EnterTokenStream(TokenArray,
                        /*DisableMacroExpansion=*/false,
                        /*IsReinject=*/false);
  }

  // `#pragma BOMBYX OVERLAP [REASSOC]` — labels the loop that follows so
  // OpenCilk2IR::VisitLabelStmt can flag it (see `__bombyx_overlap_here`).
  // Same token-injection trick as daePragma; the REASSOC clause selects the
  // `_reassoc_` spelling, which additionally permits reordering a
  // floating-point reduction so the loop can still run ahead.
  void overlapPragma(clang::Preprocessor &PP,
                     clang::PragmaIntroducer Introducer,
                     clang::Token &FirstToken) {
    bool Reassoc = false;
    clang::Token Tok;
    PP.Lex(Tok);
    while (!Tok.is(clang::tok::eod)) {
      if (Tok.is(clang::tok::identifier) && PP.getSpelling(Tok) == "REASSOC")
        Reassoc = true;
      PP.Lex(Tok);
    }

    Token LabelTok;
    LabelTok.startToken();
    LabelTok.setKind(tok::identifier);
    LabelTok.setIdentifierInfo(PP.getIdentifierInfo(
        Reassoc ? "__bombyx_overlap_reassoc_here" : "__bombyx_overlap_here"));

    Token ColTok;
    ColTok.startToken();
    ColTok.setKind(tok::colon);

    Token SemiTok;
    SemiTok.startToken();
    SemiTok.setKind(tok::semi);

    SmallVector<Token, 3> TokenList;
    TokenList.push_back(LabelTok);
    TokenList.push_back(ColTok);
    TokenList.push_back(SemiTok);

    for (Token &T : TokenList)
      T.setLocation(FirstToken.getLocation());

    ArrayRef TokenArray = TokenList;
    PP.EnterTokenStream(TokenArray,
                        /*DisableMacroExpansion=*/false,
                        /*IsReinject=*/false);
  }

  void fnIgnorePragma(clang::Preprocessor &PP,
                      clang::PragmaIntroducer Introducer,
                      clang::Token &FirstToken) {
    std::string Arg;
    clang::Token Tok;
    PP.Lex(Tok);
    if (Tok.is(clang::tok::identifier)) {
      GIgnoreFns.insert(PP.getSpelling(Tok));
    } else {
      PANIC("fuck");
      // PP.Diag(Tok.getLocation(), clang::diag::err_expected_after) <<
      // "IGNORE";
    }

    // Consume remaining tokens until end of directive
    PP.Lex(Tok);
    while (!Tok.is(clang::tok::eod)) {
      PP.Lex(Tok);
    }
  }

public:
  BombyxPragmaHandler() : PragmaHandler("BOMBYX") {}

  void HandlePragma(clang::Preprocessor &PP, clang::PragmaIntroducer Introducer,
                    clang::Token &FirstToken) override {

    clang::Token Tok;
    PP.Lex(Tok);
    // Check if we got an identifier (like "DAE")
    std::string Arg;
    if (Tok.is(clang::tok::identifier)) {
      Arg = PP.getSpelling(Tok);
    } else {
      PP.Diag(Tok.getLocation(), clang::diag::err_expected_after) << "BOMBYX";
    }

    if (Arg == "DAE") {
      daePragma(PP, Introducer, FirstToken);
    } else if (Arg == "OVERLAP") {
      overlapPragma(PP, Introducer, FirstToken);
    } else if (Arg == "IGNORE") {
      fnIgnorePragma(PP, Introducer, FirstToken);
    } else {
      PANIC("unknown bombyx pragma %s", Arg.c_str());
    }
  }
};

// Frontened action to create the custom AST consumer
class CilkConvertAction : public clang::ASTFrontendAction {
public:
  CilkConvertAction(StringRef OutFilename) : OutFilename(OutFilename) {}
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, StringRef file) override {
    clang::Preprocessor &PP = CI.getPreprocessor();
    PP.enableIncrementalProcessing();

    auto H = new BombyxPragmaHandler();
    CI.getPreprocessor().AddPragmaHandler(H);

    return std::make_unique<CilkConvert>(CI, OutFilename);
  }

private:
  StringRef OutFilename;
};

void set_target(const char *targ) {
  if (strcmp(targ, "cilk1emu") == 0) {
    GOpts.Target = ConvertOpts::TG_CILK1EMU;
  } else if (strcmp(targ, "hardcilk") == 0) {
    GOpts.Target = ConvertOpts::TG_HARDCILK;
  } else if (strcmp(targ, "tbb") == 0) {
    GOpts.Target = ConvertOpts::TG_TBB;
  } else if (strcmp(targ, "help") == 0) {
    fprintf(stderr, "Available targets: cilk1emu, tbb, hardcilk\n");
    exit(EXIT_SUCCESS);
  } else {
    PANIC("unrecognized target %s", targ);
  }
}

static bool endsWith(const std::string &s, const std::string &suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int main(int argc, char *argv[]) {
  opterr = 0;
  int c;

  static struct option long_options[] = {
      {"fdump-dot", required_argument, 0, 0},
      {"fgen-driver", no_argument, 0, 0},
      {"target", required_argument, 0, 0},
  };
  int option_index = -1;
  while ((c = getopt_long(argc, argv, "vVt:d:", long_options, &option_index)) !=
         -1) {
    switch (c) {
    case 0:
      switch (option_index) {
      case 0: {
        char *ps = optarg;
        char *p = optarg;
        do {
          char po = *p;
          if (*p == 0 || *p == ',') {
            *p = 0;
            GOpts.DumpPasses.insert(atoi(ps));
            ps = p + 1;
          }
          *p = po;
        } while (*(p++) != 0);
        break;
      }
      case 1: {
        GOpts.HCGenDriver = true;
        break;
      }
      case 2: {
        set_target(optarg);
        break;
      }
      }
      break;
    case 't':
      set_target(optarg);
      break;
    case 'd':
      GOpts.OutputDir = optarg;
      break;
    case 'v':
      VERBOSITY = 1;
      break;
    case 'V':
      VERBOSITY = 2;
      break;
    default: /* '?' */
      fprintf(
          stderr,
          "Usage: %s [OPTION]... INFILE\n"
          "Output is placed next to INFILE (or in DIR if -d is given).\n"
          "   -v                      \t verbose\n"
          "   -V                      \t very verbose\n"
          "   -d <DIR>                \t base output directory\n"
          "       --fdump-dot=<PASSES>\t Indices of passes to dump GraphViz "
          "output after, comma separated\n"
          "       --fgen-driver       \t (HardCilk only) generate driver code\n"
          "   -t, --target=<TARGET>\t Output backend. Use TARGET=help to print "
          "available\n",
          argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (argc - optind < 1) {
    std::cerr << "Expected path to input OpenCilk (C++) file." << std::endl;
    return 1;
  }

  std::string Input = argv[optind];

  bool IsCpp = endsWith(Input, ".cpp") || endsWith(Input, ".cc") ||
               endsWith(Input, ".cxx") || endsWith(Input, ".C") ||
               endsWith(Input, ".hpp") || endsWith(Input, ".hh") ||
               endsWith(Input, ".hxx");

  // Derive output path from input stem.
  // Default base dir = parent directory of the input file.
  std::filesystem::path InputPath(Input);
  std::string Stem = InputPath.stem().string();
  GOpts.AppName = Stem;

  std::filesystem::path InputParent = InputPath.parent_path();
  if (InputParent.empty())
    InputParent = ".";

  std::filesystem::path BaseOutDir =
      GOpts.OutputDir.empty() ? InputParent
                              : std::filesystem::path(GOpts.OutputDir);

  std::string OutFilename;
  if (GOpts.Target == ConvertOpts::TG_HARDCILK) {
    OutFilename = (BaseOutDir / (Stem + "_HardCilk")).string();
  } else {
    if (!GOpts.OutputDir.empty()) {
      std::error_code MkEC;
      std::filesystem::create_directories(BaseOutDir, MkEC);
      if (MkEC) {
        llvm::errs() << "error: could not create output directory "
                     << BaseOutDir.string() << ": " << MkEC.message() << "\n";
        return 1;
      }
    }
    const char *Suffix =
        GOpts.Target == ConvertOpts::TG_TBB ? "_tbb.cpp" : "_cilk1.cpp";
    OutFilename = (BaseOutDir / (Stem + Suffix)).string();
  }

  std::vector<std::string> compilationFlags = {
      IsCpp ? OPENCILK_HOME "/bin/clang++" : OPENCILK_HOME "/bin/clang",
      "-x",
      IsCpp ? "c++" : "c",
      "-c",
      "-Wall",
      "-Wno-unused-label",
      "-fopencilk",
      "-fsyntax-only",
      "-Wno-unused",
  };

  compilationFlags.push_back(Input);

  std::shared_ptr<clang::PCHContainerOperations> PCHContainerOps =
      std::make_shared<clang::PCHContainerOperations>();

  clang::FileSystemOptions FSOpts;
  llvm::IntrusiveRefCntPtr<clang::FileManager> Files(
      new clang::FileManager(FSOpts));

  clang::tooling::ToolInvocation invocation(
      compilationFlags, std::make_unique<CilkConvertAction>(OutFilename),
      Files.get(), PCHContainerOps);
  return !invocation.run();
}