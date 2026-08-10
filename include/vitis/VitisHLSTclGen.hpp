#pragma once

#include "hardcilk/HardCilkAnalysis.hpp"
#include <map>
#include <string>

// Write the Vitis HLS TCL synthesis script for a single PE kernel to Out.
//
// appName      — application name (used to locate <appName>.cpp/.h sources)
// taskName     — top-function / kernel name
// hasAXI       — if true, emits m_axi interface directives
// fpgaModel    — FPGA model string from the descriptor (e.g. "ALVEO_U55C")
// freqMhz      — target clock frequency in MHz
// dfStartFifoDepth — when non-zero, the PE was emitted as a deep-state DATAFLOW
//                region and needs `config_dataflow -start_fifo_depth` set to
//                this. It is a solution-level directive with no source-pragma
//                equivalent, which is the only reason it has to be threaded
//                down here from the HLS-code emitter; see
//                VitisHLSTarget::getDataflowStartFifoDepths.
// Out          — output stream for the .tcl file
//
// Sources are referenced relative to the script's own parent directory so that
// the generated TCL file remains relocatable alongside the .cpp/.h files.
void PrintVitisHLSTaskTcl(const std::string &AppName,
                           const std::string &TaskName,
                           bool HasAXI,
                           const std::string &FpgaModel,
                           int FreqMhz,
                           unsigned DFStartFifoDepth,
                           llvm::raw_ostream &Out);

// Write a self-contained build_hls.sh script that synthesises every
// non-synthetic PE for the application.
//
// Generates one hls_tcl/<taskName>.tcl per task, then writes build_hls.sh
// which runs vitis_hls for each PE, copies the synthesised Verilog to
// vitis_hls_output/<taskName>/, and cleans up intermediate state unless
// --debug is passed.
//
// appName    — application name
// TaskInfos  — analysis results (IsSynthetic tasks are skipped)
// fpgaModel  — FPGA model string
// freqMhz    — target clock frequency in MHz
// OutputDir  — absolute path to the HardCilk output directory
// DFStartFifoDepths — VitisHLSTarget::getDataflowStartFifoDepths(), i.e. the
//              PEs the HLS emitter turned into DATAFLOW regions and the
//              start-FIFO depth each needs. Empty unless --deep-state=dataflow
//              actually rewrote something. Must therefore be called AFTER
//              VitisHLSTarget::PrintHardCilk.
void PrintVitisHLSArtifacts(const std::string &AppName,
                             const TaskInfosTy &TaskInfos,
                             const std::string &FpgaModel,
                             int FreqMhz,
                             const std::string &OutputDir,
                             const std::map<std::string, unsigned>
                                 &DFStartFifoDepths);
