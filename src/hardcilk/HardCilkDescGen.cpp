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

static llvm::json::Object
printTaskDescriptor(IRFunction *Task, const HCTaskInfo &TaskInfo,
                    const std::string &OutputDir,
                    const IRFuncSetTy &SpawnTargets) {
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
  // Note a task can be both the root and a spawn target in re-entrant / OVERLAP
  // loops (e.g. reentry0 is spawned again by its own continuation), so gate on
  // actual spawn-target membership rather than !IsRoot.
  if (TaskInfo.IsCont || SpawnTargets.count(Task))
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
  // A task that reaches this point is scheduler-visible, so it is wired by the
  // general HardCilk interconnect like any other — the tasks the generated
  // SystemVerilog merge wrapper wires privately never get a descriptor at all.
  // There is deliberately no "overlap" key: nothing downstream reads it, and on
  // a loop whose body escapes the wrapper (its initializer, exit and loop-back
  // continuation stay external) it would flag ordinary tasks as specially
  // wired. What OVERLAP does change here is StreamingCont below, which keeps a
  // FIFO-fed continuation off the argumentNotifier/allocator sides.
  std::vector<llvm::json::Value> sidesConfigs{getSchedulerSide(TaskInfo)};
  if (TaskInfo.IsCont && !TaskInfo.StreamingCont) {
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
  // Every task that is spawned (directly or via spawn_next) by some task. Used
  // to decide which descriptors need a spawn server; a task can be a spawn
  // target even when it is the root (re-entrant / OVERLAP loops).
  IRFuncSetTy SpawnTargets;
  for (auto &[F, Info] : TaskInfos) {
    for (auto G : F->Info.SpawnList)
      SpawnTargets.insert(G);
    for (auto G : F->Info.SpawnNextList)
      SpawnTargets.insert(G);
  }

  // ── OVERLAP meta-task collapse ─────────────────────────────────────────────
  // A `#pragma BOMBYX OVERLAP` loop is emitted as a single SystemVerilog wrapper
  // (<AppName>_overlap_wrapper) that internally instantiates its PEs — the root
  // (loop entry), reentry, continuation, exit, and the dependent leaf(s) it
  // spawns (e.g. memReader). To the rest of the HardCilk system the whole loop
  // is ONE task: it is scheduled with the entry task's closure and signals
  // completion once. We therefore hide the internal PEs from the descriptor and
  // publish a single meta-task, remapping any external edge that referenced the
  // entry (or an internal task) to the wrapper.
  // One collapsed subsystem per OVERLAP loop.
  const std::vector<OverlapGroup> Groups = computeOverlapGroups(TaskInfos);

  // Map an internal task reference to its own wrapper for external edges.
  auto edgeName = [&](IRFunction *G) -> std::string {
    if (const OverlapGroup *Grp = findOverlapGroup(Groups, G))
      return Grp->WrapperName;
    return G->getName();
  };

  for (auto &[F, Info] : TaskInfos) {
    // Internal PEs are represented by the single meta-task, not individually.
    if (findOverlapGroup(Groups, F))
      continue;
    taskDescriptors.push_back(
        printTaskDescriptor(F, Info, AbsOutputDir, SpawnTargets));
    if (Info.HasAXI)
      anyAXI = true;
    // Only include entries with non-empty lists
    std::vector<llvm::json::Value> spawnListF;
    for (auto G : F->Info.SpawnList)
      spawnListF.push_back(llvm::json::Value(edgeName(G)));
    if (!spawnListF.empty())
      spawnList[F->getName()] = std::move(spawnListF);

    std::vector<llvm::json::Value> spawnNextListF;
    for (auto G : F->Info.SpawnNextList)
      spawnNextListF.push_back(llvm::json::Value(edgeName(G)));
    if (!spawnNextListF.empty())
      spawnNextList[F->getName()] = std::move(spawnNextListF);

    // Only emit sendArgumentList for tasks that actually have an argOut port.
    // Tasks that tail-spawn (non-empty SpawnList or SpawnNextList) forward _cont
    // through the spawned task struct rather than via argOut.
    bool NeedsArgOut = F->Info.SpawnList.empty() && F->Info.SpawnNextList.empty();
    if (NeedsArgOut) {
      std::vector<llvm::json::Value> sendArgumentListF;
      for (auto G : Info.SendArgList)
        sendArgumentListF.push_back(llvm::json::Value(edgeName(G)));
      if (!sendArgumentListF.empty())
        sendArgumentList[F->getName()] = std::move(sendArgumentListF);
    }
  }

  // Emit one meta-task descriptor per OVERLAP wrapper.
  for (const OverlapGroup &Grp : Groups) {
    IRFunction *Entry = Grp.Entry;
    const IRFuncSetTy &Internal = Grp.Internal;
    const std::string &WrapperName = Grp.WrapperName;
    const HCTaskInfo &EI = TaskInfos.at(Entry);
    int64_t entryWidth = (int64_t)(EI.TaskSize + EI.TaskPadding) * 8;
    bool metaAXI = false;
    for (IRFunction *G : Internal)
      metaAXI |= TaskInfos.at(G).HasAXI;
    anyAXI |= metaAXI;

    // The wrapper is published as an ordinary task descriptor: its RTL (the
    // generated Verilog wrapper plus its collapsed sub-PEs) lives in a folder
    // named after the wrapper under vitis_hls_output/, exactly like a normal PE.
    // The descriptor therefore carries the same key set as printTaskDescriptor's
    // output — no wrapper-specific metadata — and is scheduled with the entry
    // task's closure type.
    llvm::json::Object meta;
    meta["name"] = WrapperName;
    meta["peHDLPath"] = AbsOutputDir + "/vitis_hls_output/" + WrapperName;
    meta["isRoot"] = EI.IsRoot;
    meta["isCont"] = false;
    meta["hasAXI"] = metaAXI;
    meta["numProcessingElements"] = peCountFor(WrapperName);
    meta["dynamicMemAlloc"] = false;
    meta["widthTask"] = entryWidth;
    meta["widthMalloc"] = 0;
    meta["variableSpawn"] = false;
    // spawnServersCount: the wrapper needs a spawn server only if it is spawned
    // from OUTSIDE the collapsed subsystem. Its entry being spawned by an internal
    // task (the loop's own continuation) is handled inside the wrapper and needs
    // no server. A root-only overlap loop (host-injected, nothing external spawns
    // it) therefore gets 0, exactly like a normal root task — mismatching this is
    // what over-sizes the downstream management interconnect.
    bool ExternallySpawned = false;
    for (auto &[F, Info2] : TaskInfos) {
      if (Internal.count(F))
        continue;
      for (auto G : F->Info.SpawnList)
        if (Internal.count(G))
          ExternallySpawned = true;
      for (auto G : F->Info.SpawnNextList)
        if (Internal.count(G))
          ExternallySpawned = true;
    }
    if (ExternallySpawned)
      meta["spawnServersCount"] = 1;
    // generateArgOutWriteBuffer: emitted for all non-root tasks, mirroring
    // printTaskDescriptor. The wrapper passes its internal leaf sender's argOut
    // and argDataOut straight out on top-level ports, so it needs a write buffer
    // exactly when that leaf sender does — the wrapper RTL already keys its
    // argDataOut port on the same flag (HardCilkOverlapWrapperGen's
    // ExitHasArgData), and hard-coding false here made the descriptor disagree
    // with the RTL it describes.
    if (!EI.IsRoot) {
      bool GenArgOutBuf = false;
      std::vector<IRFunction *> InternalV(Internal.begin(), Internal.end());
      llvm::sort(InternalV, [](IRFunction *A, IRFunction *B) {
        return A->getName() < B->getName();
      });
      for (IRFunction *G : InternalV) {
        auto It = TaskInfos.find(G);
        if (It == TaskInfos.end() || !It->second.GenerateArgOutWriteBuffer)
          continue;
        // Only a leaf sender owns an argOut port; a task that tail-spawns or
        // owns a spawn_next forwards _cont through the closure instead.
        if (!G->Info.SpawnList.empty() || !G->Info.SpawnNextList.empty())
          continue;
        for (IRFunction *H : It->second.SendArgList)
          if (!Internal.count(H))
            GenArgOutBuf = true;
      }
      meta["generateArgOutWriteBuffer"] = GenArgOutBuf;
    }
    // One scheduler side sized to the entry closure; the wrapper handles all the
    // internal streaming itself, so it needs no argumentNotifier/allocator.
    llvm::json::Object sched;
    sched["sideType"] = "scheduler";
    sched["numVirtualServers"] = 1;
    sched["capacityVirtualQueue"] = 4096;
    sched["capacityPhysicalQueue"] = 64;
    sched["portWidth"] = entryWidth;
    meta["sidesConfigs"] =
        std::vector<llvm::json::Value>{llvm::json::Value(std::move(sched))};
    taskDescriptors.push_back(llvm::json::Value(std::move(meta)));

    // External edges of the subsystem: anything an internal task spawns or sends
    // to that is NOT itself internal is an edge from the wrapper.
    std::set<std::string> extSpawn, extSpawnNext, extSend;
    // Payload width (bits) carried on each external argDataOut, keyed by the
    // external destination name — mirrors printTaskDescriptor's per-port sizing
    // so the wrapper's exposed argDataOut is consumed like a normal leaf sender.
    std::map<std::string, int64_t> extSendBits;
    for (IRFunction *G : Internal) {
      for (auto H : G->Info.SpawnList)
        if (!Internal.count(H))
          extSpawn.insert(edgeName(H));
      // A spawn_next leaving the subsystem keeps its own mechanics: the
      // wrapper exposes the closureIn / spawnNext ports of the internal task
      // that issues it, so it must be published as a spawn_next edge, not a
      // plain spawn.
      for (auto H : G->Info.SpawnNextList)
        if (!Internal.count(H))
          extSpawnNext.insert(edgeName(H));
      auto It = TaskInfos.find(G);
      if (It == TaskInfos.end())
        continue;
      const HCTaskInfo &GI = It->second;
      // Only leaf senders actually emit argDataOut (see printTaskDescriptor).
      bool NeedsVoidSend =
          G->Info.SpawnNextList.empty() && G->Info.SpawnList.empty();
      bool RetIsValue = GI.RetTy && !typeIsVoid(*GI.RetTy);
      bool NeedsArgData = RetIsValue || GI.GenerateArgOutWriteBuffer;
      int64_t Bits = RetIsValue
                         ? (int64_t)hardCilkTypeSize(GI.RetTy.get()) * 8
                         : (int64_t)GI.BufferedArgumentBits;
      // Only a leaf sender actually has an argOut port to expose. An internal
      // task that tail-spawns or owns a spawn_next forwards _cont through the
      // spawned closure instead, so its SendArgList must not become a
      // sendArgument edge of the wrapper.
      if (!NeedsVoidSend)
        continue;
      for (auto H : GI.SendArgList)
        if (!Internal.count(H)) {
          extSend.insert(edgeName(H));
          if (!GI.IsRoot && NeedsArgData)
            extSendBits[edgeName(H)] = Bits;
        }
    }
    if (!extSpawn.empty()) {
      std::vector<llvm::json::Value> v;
      for (auto &n : extSpawn)
        v.push_back(llvm::json::Value(n));
      spawnList[WrapperName] = std::move(v);
    }
    if (!extSpawnNext.empty()) {
      std::vector<llvm::json::Value> v;
      for (auto &n : extSpawnNext)
        v.push_back(llvm::json::Value(n));
      spawnNextList[WrapperName] = std::move(v);
    }
    if (!extSend.empty()) {
      std::vector<llvm::json::Value> v;
      for (auto &n : extSend)
        v.push_back(llvm::json::Value(n));
      sendArgumentList[WrapperName] = std::move(v);
    }
    // argumentSizeList: the wrapper exposes the exit's external argDataOut ports,
    // named by the same convention as a normal PE — plain "argDataOut" for a
    // single external destination, else "argDataOut_<dest>".
    if (!extSendBits.empty()) {
      bool Multi = extSend.size() > 1;
      llvm::json::Object ArgumentSizeList;
      for (auto &[Name, Bits] : extSendBits) {
        std::string Port = Multi ? "argDataOut_" + Name : std::string("argDataOut");
        ArgumentSizeList[Port] = Bits;
      }
      // Attach to the wrapper's own descriptor (already appended above).
      for (auto &TdV : taskDescriptors)
        if (auto *Obj = TdV.getAsObject())
          if (auto N = Obj->getString("name"); N && *N == WrapperName) {
            (*Obj)["argumentSizeList"] = std::move(ArgumentSizeList);
            break;
          }
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
