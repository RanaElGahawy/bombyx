#pragma once

#include "hardcilk/HardCilkAnalysis.hpp"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <string>

// Per-task PE-count overrides, keyed by task name (the `--pes` flag). Absent
// names keep the default of 1. Replication is a system-level knob, not a
// property of the kernel source, so it has to come in from outside; before this
// existed the only way to get a replicated PE was to hand-edit the emitted
// descriptor, which every regeneration then silently undid.
extern std::map<std::string, int> HCPECounts;

void PrintHardCilkDescJson(const std::string &AppName,
                           const TaskInfosTy &TaskInfos,
                           const std::string &OutputDir,
                           llvm::raw_ostream &Out);
