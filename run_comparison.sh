#!/usr/bin/env bash
#
# Build the sequential and MPI solvers, run both, and compare their final
# grids using the max_error metric from Section 6 of the project PDF.
#
# Usage:
#   ./run_comparison.sh [ranks]
#
# distributed-hosts.cxx currently assumes exactly two ranks.

set -euo pipefail

RANKS="${1:-2}"
if [[ "$RANKS" != "2" ]]; then
  echo "error: distributed-hosts.cxx only supports 2 ranks (got $RANKS)" >&2
  exit 2
fi

make all

echo
echo "== Running sequential solver =="
./seq --dump seq.bin

echo
echo "== Running MPI solver ($RANKS ranks) =="
mpiexec -n "$RANKS" ./dh --dump mpi.bin

echo
echo "== Comparing final grids =="
set +e
./compare seq.bin mpi.bin
status=$?
set -e

exit "$status"
