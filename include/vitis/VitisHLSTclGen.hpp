#pragma once

#include "hardcilk/HardCilkAnalysis.hpp"
#include <string>

// Write the Vitis HLS TCL synthesis script for a single PE kernel to Out.
//
// appName      — application name (used to locate <appName>.cpp/.h sources)
// taskName     — top-function / kernel name
// hasAXI       — if true, emits m_axi interface directives
// fpgaModel    — FPGA model string from the descriptor (e.g. "ALVEO_U55C")
// freqMhz      — target clock frequency in MHz
// Out          — output stream for the .tcl file
//
// Sources are referenced relative to the script's own parent directory so that
// the generated TCL file remains relocatable alongside the .cpp/.h files.
void PrintVitisHLSTaskTcl(const std::string &AppName,
                           const std::string &TaskName,
                           bool HasAXI,
                           const std::string &FpgaModel,
                           int FreqMhz,
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
void PrintVitisHLSArtifacts(const std::string &AppName,
                             const TaskInfosTy &TaskInfos,
                             const std::string &FpgaModel,
                             int FreqMhz,
                             const std::string &OutputDir);
