#pragma once

#include "core/IR.hpp"

// Remove ARG variables from spawned task functions that are never referenced
// in the function body, pruning the corresponding fields from the task struct
// and the matching arguments from every ESpawnIRStmt / ClosureDeclIRStmt that
// calls into the pruned function.
void PruneDeadSpawnArgs(IRProgram &P);
