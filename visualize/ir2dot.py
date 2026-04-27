#!/usr/bin/env python3
import json
import sys

data = json.load(sys.stdin)


def esc(s):
    return s.replace('\\', '\\\\').replace('"', '\\"').replace('<', '\\<').replace('>', '\\>').replace('{', '\\{').replace('}', '\\}').replace('|', '\\|')


print('digraph IR {')
print('  node [shape=record fontname="Courier" fontsize=11]')
print('  rankdir=TB')
print('  compound=true')

for fn in data["functions"]:
    fname = fn["name"]
    # subgraph name must start with "cluster_" for Graphviz to draw a box
    print(f'  subgraph cluster_{fname} {{')
    print(
        f'    label="{fname}" fontname="Courier" fontsize=13 fontcolor="#333333"')
    print(f'    style=rounded color="#888888"')

    for block in fn["blocks"]:
        bid = block["id"]
        node_id = f'{fname}_{bid}'   # unique across functions
        instrs = block["instrs"]
        term = block["terminator"] or ""

        instr_rows = "".join(f"{esc(i)}\\l" for i in instrs)
        term_row = f"T: {esc(term)}\\l" if term else ""
        label = f"{{{bid}|{instr_rows}{term_row}}}"

        is_entry = (block == fn["blocks"][0])
        is_exit = len(block["succs"]) == 0

        if is_entry:
            color = 'style=filled fillcolor="#cce5ff"'
        elif is_exit:
            color = 'style=filled fillcolor="#ffcccc"'
        else:
            color = 'style=filled fillcolor="#ffffff"'

        print(f'    {node_id} [label="{label}" {color}]')

    print('  }')

    # Edges (outside subgraph so cross-function refs work too)
    for block in fn["blocks"]:
        bid = block["id"]
        node_id = f'{fname}_{bid}'
        term = block["terminator"] or ""
        is_branch = term.startswith("if")

        for i, succ in enumerate(block["succs"]):
            succ_id = f'{fname}_{succ}'
            lbl = ""
            if is_branch:
                lbl = 'label="true"' if i == 0 else 'label="false"'
            print(f'  {node_id} -> {succ_id} [{lbl}]')

print('}')
