#pragma once

#include "core/IR.hpp"
#include "hardcilk/HardCilkAnalysis.hpp"
#include "llvm/Support/raw_ostream.h"

#include <string>

// Emit a SystemVerilog top-level wrapper for a `#pragma BOMBYX OVERLAP` loop.
//
// The wrapper instantiates the Vitis-HLS-synthesized processing elements
// (root / reentry / memReader / continuation / exit) and wires them for
// in-order streaming continuation delivery:
//
//   * reentry pushes the loop-carried continuation state onto an on-chip FIFO
//     (its `contStateOut_<cont>` AXI-Stream port) and issues the per-iteration
//     memory reads;
//   * the single in-order memory-reply channel (memReader `argDataOut`) is
//     gathered N replies at a time and merged with the head of the state FIFO;
//   * the merged closure drives the continuation PE's `taskIn`, which loops
//     back to reentry.
//
// Returns true iff an OVERLAP loop was found and a wrapper was emitted; when it
// returns false nothing was written and the caller should skip wrapper output.
// Emits the wrapper for ONE OVERLAP loop; call once per group returned by
// computeOverlapGroups.
bool PrintHardCilkOverlapWrapper(const std::string &AppName, IRProgram &P,
                                 const TaskInfosTy &TaskInfos,
                                 const OverlapGroup &Grp,
                                 llvm::raw_ostream &Out);
