#include "hardcilk/HardCilkDescGen.hpp"
#include "core/IR.hpp"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <vector>

std::map<std::string, int> HCPECounts;

// PE count for a task: the `--pes` override if one was given, else 1.
static int peCountFor(llvm::StringRef Name) {
  auto It = HCPECounts.find(Name.str());
  return It == HCPECounts.end() ? 1 : It->second;
}

static llvm::json::Object getSchedulerSide(const HCTaskInfo &TaskInfo) {
  llvm::json::Object obj;
  obj["sideType"] = "scheduler";
  obj["numVirtualServers"] = 1;
  obj["capacityVirtualQueue"] = 4096;
  obj["capacityPhysicalQueue"] = 64;
  int64_t totalSize = (TaskInfo.TaskSize + TaskInfo.TaskPadding) * 8;
  obj["portWidth"] = totalSize;
  return obj;
}

static llvm::json::Object getArgumentNotifierSide() {
  llvm::json::Object obj;
  obj["sideType"] = "argumentNotifier";
  obj["numVirtualServers"] = 1;
  obj["capacityVirtualQueue"] = 128;
  obj["capacityPhysicalQueue"] = 32;
  obj["portWidth"] = 64;
  return obj;
}

static llvm::json::Object getAllocatorSide() {
  llvm::json::Object obj;
  obj["sideType"] = "allocator";
  obj["numVirtualServers"] = 1;
  obj["capacityVirtualQueue"] = 4096;
  obj["capacityPhysicalQueue"] = 32;
  obj["portWidth"] = 64;
  return obj;
}

static llvm::json::Object printTaskDescriptor(IRFunction *Task,
                                              const HCTaskInfo &TaskInfo,
                                              const std::string &OutputDir) {
  llvm::json::Object obj;
  obj["name"] = Task->getName();
  obj["peHDLPath"] = OutputDir + "/vitis_hls_output/" + Task->getName();
  obj["isRoot"] = TaskInfo.IsRoot;
  obj["isCont"] = TaskInfo.IsCont;
  obj["hasAXI"] = TaskInfo.HasAXI;
  obj["numProcessingElements"] = peCountFor(Task->getName());
  obj["dynamicMemAlloc"] = false;
  int64_t closureSize = (int64_t)(TaskInfo.TaskSize + TaskInfo.TaskPadding) * 8;
  obj["widthTask"] = closureSize;
  obj["widthMalloc"] = 0;
  obj["variableSpawn"] = false;
  // spawnServersCount: required whenever the task is spawned by another task
  // or is a continuation; the scheduler must reserve at least one spawn server.
  if (TaskInfo.IsCont || !TaskInfo.IsRoot)
    obj["spawnServersCount"] = 1;
  // generateArgOutWriteBuffer: emit for all non-root tasks (false is meaningful)
  if (!TaskInfo.IsRoot)
    obj["generateArgOutWriteBuffer"] = TaskInfo.GenerateArgOutWriteBuffer;

  // argumentSizeList: payload width (bits) carried on each argDataOut port,
  // keyed by the exact stream name written in the PE so every entry maps to its
  // related write. A task's argData payload is its return type (non-void tasks)
  // or its buffered-store type; the same width is written to every destination
  // port, but each port gets its own entry. Single-destination tasks use the
  // plain "argDataOut" port; multi-destination tasks use "argDataOut_<cont>".
  // Only leaf senders (empty spawn-lists) actually have argDataOut ports; a task
  // that tail-spawns forwards its argument through the spawned closure and never
  // writes argOut/argDataOut. This mirrors the PE port emission and the JSON
  // sendArgumentList gating.
  bool NeedsVoidSend =
      Task->Info.SpawnNextList.empty() && Task->Info.SpawnList.empty();
  if (!TaskInfo.IsRoot && !TaskInfo.SendArgList.empty() && NeedsVoidSend) {
    bool RetIsValue = TaskInfo.RetTy && !typeIsVoid(*TaskInfo.RetTy);
    bool NeedsArgData = RetIsValue || TaskInfo.GenerateArgOutWriteBuffer;
    if (NeedsArgData) {
      int64_t Bits = RetIsValue
                         ? (int64_t)hardCilkTypeSize(TaskInfo.RetTy.get()) * 8
                         : (int64_t)TaskInfo.BufferedArgumentBits;
      bool Multi = TaskInfo.SendArgList.size() > 1;
      std::vector<IRFunction *> Dests(TaskInfo.SendArgList.begin(),
                                      TaskInfo.SendArgList.end());
      std::sort(Dests.begin(), Dests.end(), [](IRFunction *A, IRFunction *B) {
        return A->getName() < B->getName();
      });
      llvm::json::Object ArgumentSizeList;
      for (IRFunction *D : Dests) {
        std::string Port =
            Multi ? "argDataOut_" + D->getName() : std::string("argDataOut");
        ArgumentSizeList[Port] = Bits;
      }
      obj["argumentSizeList"] = std::move(ArgumentSizeList);
    }
  }
  // 8-bit tag identifying this continuation type. The framework encodes it into
  // the high 8 bits of the continuation's closure address so that a task sending
  // to multiple continuations can route by it.
  if (TaskInfo.IsCont) {
    obj["tag"] = (int64_t)TaskInfo.Tag;
    // Number of ordered write-buffer beats this continuation's closure is
    // written in (1 = fits in one beat; >1 = closure wider than the buffer's
    // per-beat payload, split into that many sequential spawn_next writes).
    obj["closureWriteBeats"] = (int64_t)closureWriteBeats(TaskInfo);
  }
  std::vector<llvm::json::Value> sidesConfigs{getSchedulerSide(TaskInfo)};
  if (TaskInfo.IsCont) {
    sidesConfigs.push_back(getArgumentNotifierSide());
    sidesConfigs.push_back(getAllocatorSide());
  }
  obj["sidesConfigs"] = sidesConfigs;
  return obj;
}

void PrintHardCilkDescJson(const std::string &AppName,
                           const TaskInfosTy &TaskInfos,
                           const std::string &OutputDir,
                           llvm::raw_ostream &Out) {
  // peHDLPath must be a complete absolute path: the downstream architecture
  // generator resolves it with `new File(peHDLPath).exists`, so a `-d` given as a
  // relative directory (resolved against the compiler's CWD) must be absolutized
  // here rather than echoed verbatim.
  std::error_code AbsEC;
  std::filesystem::path AbsPath =
      std::filesystem::absolute(std::filesystem::path(OutputDir), AbsEC);
  const std::string AbsOutputDir =
      AbsEC ? OutputDir : AbsPath.lexically_normal().string();

  llvm::json::Object obj;
  obj["name"] = AppName;
  std::vector<llvm::json::Value> taskDescriptors;
  llvm::json::Object spawnList;
  llvm::json::Object spawnNextList;
  llvm::json::Object sendArgumentList;
  llvm::json::Object mallocList;
  bool anyAXI = false;
  for (auto &[F, Info] : TaskInfos) {
    taskDescriptors.push_back(printTaskDescriptor(F, Info, OutputDir));
    if (Info.HasAXI)
      anyAXI = true;
    // Only include entries with non-empty lists
    std::vector<llvm::json::Value> spawnListF;
    for (auto G : F->Info.SpawnList)
      spawnListF.push_back(llvm::json::Value(G->getName()));
    if (!spawnListF.empty())
      spawnList[F->getName()] = std::move(spawnListF);

    std::vector<llvm::json::Value> spawnNextListF;
    for (auto G : F->Info.SpawnNextList)
      spawnNextListF.push_back(llvm::json::Value(G->getName()));
    if (!spawnNextListF.empty())
      spawnNextList[F->getName()] = std::move(spawnNextListF);

    // Only emit sendArgumentList for tasks that actually have an argOut port.
    // Tasks that tail-spawn (non-empty SpawnList or SpawnNextList) forward _cont
    // through the spawned task struct rather than via argOut.
    bool NeedsArgOut = F->Info.SpawnList.empty() && F->Info.SpawnNextList.empty();
    if (NeedsArgOut) {
      std::vector<llvm::json::Value> sendArgumentListF;
      for (auto G : Info.SendArgList)
        sendArgumentListF.push_back(llvm::json::Value(G->getName()));
      if (!sendArgumentListF.empty())
        sendArgumentList[F->getName()] = std::move(sendArgumentListF);
    }
  }
  obj["taskDescriptors"] = taskDescriptors;
  obj["spawnList"] = llvm::json::Value(std::move(spawnList));
  obj["spawnNextList"] = llvm::json::Value(std::move(spawnNextList));
  obj["sendArgumentList"] = llvm::json::Value(std::move(sendArgumentList));
  obj["mallocList"] = llvm::json::Value(std::move(mallocList));
  obj["widthAddress"] = 64;
  obj["widthContCounter"] = 32;
  obj["memorySizeSim"] = 16;
  obj["targetFrequency"] = 300;
  obj["fpgaModel"] = "ALVEO_U55C";
  obj["isVitisProject"] = true;
  if (anyAXI)
    obj["widthAXIAddress"] = 34;
  llvm::json::Value objV(std::move(obj));
  Out << llvm::formatv("{0:2}", objV);
}
