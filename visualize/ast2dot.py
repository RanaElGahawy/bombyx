#!/usr/bin/env python3
# ast2dot.py
import json
import sys

data = json.load(sys.stdin)

# Node kinds to skip (clutter without adding info)
SKIP_KINDS = {"TranslationUnitDecl", "FullComment", "ParagraphComment",
              "TextComment", "CompoundStmt"}  # remove CompoundStmt if you want it

node_count = [0]


def fresh_id():
    node_count[0] += 1
    return f"n{node_count[0]}"


def esc(s):
    # truncate long labels
    return str(s).replace('"', '\\"').replace('\n', ' ')[:60]


def label_for(node):
    kind = node.get("kind", "?")
    parts = [kind]

    if "name" in node:
        parts.append(node["name"])
    if "opcode" in node:
        parts.append(node["opcode"])
    if "value" in node:
        parts.append(str(node["value"]))
    if "referencedDecl" in node:
        parts.append(node["referencedDecl"].get("name", ""))
    if "castKind" in node:
        parts.append(node["castKind"])
    if "type" in node:
        t = node["type"].get("qualType", "")
        if t:
            parts.append(f":{t}")

    return esc(" ".join(parts))


def walk(node, parent_id, lines):
    kind = node.get("kind", "")
    if kind in SKIP_KINDS:
        # still walk children but don't emit a node
        for child in node.get("inner", []):
            walk(child, parent_id, lines)
        return

    nid = fresh_id()
    lbl = label_for(node)

    # Color by category
    if "Decl" in kind:
        color = 'fillcolor="#cce5ff"'
    elif "Stmt" in kind or kind == "CompoundStmt":
        color = 'fillcolor="#d4edda"'
    elif "Expr" in kind or "Literal" in kind or "Operator" in kind:
        color = 'fillcolor="#fff3cd"'
    else:
        color = 'fillcolor="#f8f9fa"'

    lines.append(f'  {nid} [label="{lbl}" style=filled {color}]')
    if parent_id:
        lines.append(f'  {parent_id} -> {nid}')

    for child in node.get("inner", []):
        walk(child, nid, lines)


lines = []
lines.append('digraph AST {')
lines.append(
    '  node [shape=box fontname="Courier" fontsize=10 margin="0.1,0.05"]')
lines.append('  rankdir=TB')
lines.append('  edge [arrowsize=0.7]')

# The top-level node is the TranslationUnit — skip it, walk its children directly
for top in data.get("inner", []):
    if top.get("kind") == "FunctionDecl" and top.get("name") in ("fib"):
        walk(top, None, lines)

lines.append('}')
print("\n".join(lines))
