#!/bin/bash
# Golden regression check for the OVERLAP wrapper generator.
#
# The wrapper emitter is being generalised from a single linear round-chain to a
# general dataflow graph. The single-round (countIntersections) and escaping
# (applyFn_reentry0) wrappers must stay BYTE-IDENTICAL through that refactor, so
# they are frozen here. Regenerate with --update after an intentional change.
#
# peHDLPath in the descriptor is an absolute path derived from the output dir, so
# it is normalised away before comparing.

set -e

BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ROOT="$(cd "$BASE/.." && pwd)"
BOMBYX="$BASE/build/bin/bombyx-cc"
GOLD="$BASE/tests/golden"
UPDATE=0
[ "$1" = "--update" ] && UPDATE=1

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

norm() { sed 's#"peHDLPath": *"[^"]*"#"peHDLPath": "<abs>"#' "$1"; }

fail=0
check_app() { # <app-name> <source> <artifact...>
  local NAME="$1" SRC="$2"; shift 2
  echo ">> golden: $NAME"
  "$BOMBYX" -t hardcilk -d "$WORK" "$SRC" > "$WORK/$NAME.log" 2>&1
  local OUT="$WORK/${NAME}_HardCilk"
  for f in "$@"; do
    if [ $UPDATE -eq 1 ]; then
      mkdir -p "$GOLD/$NAME"
      cp "$OUT/$f" "$GOLD/$NAME/$f"
      echo "   updated $f"
      continue
    fi
    if [ ! -f "$GOLD/$NAME/$f" ]; then
      echo "   MISSING golden $NAME/$f"; fail=1; continue
    fi
    case "$f" in
      *.json) diff <(norm "$GOLD/$NAME/$f") <(norm "$OUT/$f") > /dev/null \
                || { echo "   DIFF $f"; diff <(norm "$GOLD/$NAME/$f") <(norm "$OUT/$f") | head -40; fail=1; } ;;
      *)      diff "$GOLD/$NAME/$f" "$OUT/$f" > /dev/null \
                || { echo "   DIFF $f"; diff "$GOLD/$NAME/$f" "$OUT/$f" | head -40; fail=1; } ;;
    esac
  done
}

check_app triangleDAEOptimal \
  "$ROOT/Bombyx_OpenCilk_Examples/triangleDAEFull/triangleDAEOptimal.cpp" \
  countIntersections_overlap_wrapper.v \
  applyFn_reentry0_overlap_wrapper.v \
  triangleDAEOptimal.json

# The nested (compact) case has no frozen RTL yet — it is the shape under active
# development — but its descriptor must satisfy the Scala generator's invariants
# and must collapse to exactly three scheduler-visible tasks.
echo ">> nested: triangleDAEOptimalCompact"
"$BOMBYX" -t hardcilk -d "$WORK" \
  "$ROOT/Bombyx_OpenCilk_Examples/triangleDAEFull/triangleDAEOptimalCompact.cpp" \
  > "$WORK/compact.log" 2>&1
CJ="$WORK/triangleDAEOptimalCompact_HardCilk/triangleDAEOptimalCompact.json"
python3 "$BASE/tests/check_desc.py" "$CJ" || fail=1
N=$(python3 -c "import json;print(len(json.load(open('$CJ'))['taskDescriptors']))")
if [ "$N" != "3" ]; then
  echo "   nested app has $N scheduler-visible tasks, expected 3"
  fail=1
fi

if [ $UPDATE -eq 0 ]; then
  for f in "$GOLD"/*/*.json; do
    python3 "$BASE/tests/check_desc.py" "$f" || fail=1
  done
fi

# Run-ahead legality gate: each input asserts one verdict. Rejection is not an
# error (the loop just keeps serial outer iteration), so it is only visible as a
# diagnostic — which makes it exactly the kind of thing that regresses silently.
echo ">> run-ahead legality"
check_runahead() { # <source> <expect: accept|substring of the rejection reason>
  local SRC="$BASE/tests/overlap/$1" EXPECT="$2"
  local OUT
  OUT=$("$BOMBYX" -t hardcilk -d "$WORK" "$SRC" 2>&1 |
        sed -n 's/^note: #pragma BOMBYX OVERLAP loop cannot run ahead: //p' |
        sed 's/;.*//')
  if [ "$EXPECT" = "accept" ]; then
    if [ -n "$OUT" ]; then
      echo "   $1: expected run-ahead ACCEPTED, got rejection: $OUT"; fail=1
    fi
  else
    case "$OUT" in
      *"$EXPECT"*) ;;
      "") echo "   $1: expected rejection '$EXPECT', but run-ahead was accepted"; fail=1 ;;
      *)  echo "   $1: expected rejection '$EXPECT', got: $OUT"; fail=1 ;;
    esac
  fi
}
check_runahead runahead_int_reduction.cpp           accept
check_runahead runahead_float_reduction.cpp        "floating-point reduction"
check_runahead runahead_float_reduction_reassoc.cpp accept
# A store does not block run-ahead when it is provably iteration-private, but
# every way of losing that proof has to keep blocking it.
check_runahead runahead_store_in_body.cpp   accept
check_runahead runahead_store_scatter.cpp   "not injective in the induction variable"
check_runahead runahead_store_neighbour.cpp "an address another iteration writes"
check_runahead runahead_ref_call.cpp        "by mutable reference"

# The target case: applyFn's OVERLAP for-loop reduces `count` with integer `+`
# and its body neither stores nor spawns, so its iterations may be issued ahead
# of one another.
RA=$("$BOMBYX" -t hardcilk -d "$WORK" \
     "$ROOT/Bombyx_OpenCilk_Examples/triangleDAEFull/triangleDAEOptimalCompact.cpp" 2>&1 |
     sed -n 's/^note: #pragma BOMBYX OVERLAP loop cannot run ahead: //p')
if [ -n "$RA" ]; then
  echo "   triangleDAEOptimalCompact: expected run-ahead ACCEPTED, got: $RA"
  fail=1
fi

if [ $fail -ne 0 ]; then
  echo "golden check FAILED"
  exit 1
fi
echo "golden check OK"
