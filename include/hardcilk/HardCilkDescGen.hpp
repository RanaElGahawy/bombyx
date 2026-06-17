#pragma once

#include "hardcilk/HardCilkAnalysis.hpp"
#include "llvm/Support/raw_ostream.h"
#include <string>

void PrintHardCilkDescJson(const std::string &AppName,
                           const TaskInfosTy &TaskInfos,
                           const std::string &OutputDir,
                           llvm::raw_ostream &Out);
