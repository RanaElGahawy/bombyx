#include "HardCilkDescGen.hpp"
#include "IR.hpp"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"

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
                                              const HCTaskInfo &TaskInfo) {
  llvm::json::Object obj;
  obj["name"] = Task->getName();
  obj["peHDLPath"] = "?";
  obj["isRoot"] = TaskInfo.IsRoot;
  obj["isCont"] = TaskInfo.IsCont;
  obj["dynamicMemAlloc"] = false;
  int64_t closureSize = (int64_t)(TaskInfo.TaskSize + TaskInfo.TaskPadding) * 8;
  obj["widthTask"] = closureSize;
  obj["widthMalloc"] = 0;
  obj["variableSpawn"] = false;
  std::vector<llvm::json::Value> sidesConfigs{getSchedulerSide(TaskInfo)};
  if (TaskInfo.IsCont) {
    sidesConfigs.push_back(getArgumentNotifierSide());
    sidesConfigs.push_back(getAllocatorSide());
  }
  if (TaskInfo.GenerateArgOutWriteBuffer) {
    obj["generateArgOutWriteBuffer"] = true;
    std::vector<llvm::json::Value> ArgumentSizeList;
    ArgumentSizeList.push_back(
        llvm::json::Value(static_cast<int64_t>(TaskInfo.BufferedArgumentBits)));
    obj["argumentSizeList"] = std::move(ArgumentSizeList);
  }
  obj["sidesConfigs"] = sidesConfigs;
  return obj;
}

void PrintHardCilkDescJson(const std::string &AppName,
                           const TaskInfosTy &TaskInfos,
                           llvm::raw_ostream &Out) {
  llvm::json::Object obj;
  obj["name"] = AppName;
  std::vector<llvm::json::Value> taskDescriptors;
  llvm::json::Object spawnList;
  llvm::json::Object spawnNextList;
  llvm::json::Object sendArgumentList;
  llvm::json::Object mallocList;
  for (auto &[F, Info] : TaskInfos) {
    taskDescriptors.push_back(printTaskDescriptor(F, Info));
    std::vector<llvm::json::Value> spawnListF;
    for (auto G : F->Info.SpawnList) {
      spawnListF.push_back(llvm::json::Value(G->getName()));
    }
    std::vector<llvm::json::Value> spawnNextListF;
    for (auto G : F->Info.SpawnNextList) {
      spawnNextListF.push_back(llvm::json::Value(G->getName()));
    }
    std::vector<llvm::json::Value> sendArgumentListF;
    for (auto G : Info.SendArgList) {
      sendArgumentListF.push_back(llvm::json::Value(G->getName()));
    }
    spawnList[F->getName()] = spawnListF;
    spawnNextList[F->getName()] = spawnNextListF;
    sendArgumentList[F->getName()] = sendArgumentListF;
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
  llvm::json::Value objV(std::move(obj));
  Out << llvm::formatv("{0:2}", objV);
}
