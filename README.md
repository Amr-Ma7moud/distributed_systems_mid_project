# Distributed Heat Diffusion Simulation Using MPI

C++17 implementation of the *Distributed Systems Project 1* brief: simulate heat
diffusion on a 2,000 × 2,000 metal plate with a sequential program, then with an
MPI program, and prove that the two final grids agree.

The project brief is in [`Distributed_Systems_Project1.pdf`](Distributed_Systems_Project1.pdf).

## Repository layout

| Path | Purpose |
| --- | --- |
| `seq.cxx` | Sequential baseline solver (1 process). |
| `distributed-hosts.cxx` | MPI solver intended for 2 physical laptops / 2 ranks. |
| `distributed.cxx` | Small 12 × 12 teaching example (4 ranks) used to demonstrate the grid/ghost-row idea. It is **not** the 2,000 × 2,000 benchmark. |
| `compare.cxx` | Reads two dumped grids and computes the Section 6 `max_error` metric. |
| `grid_io.h` | Shared binary grid dump/read format used by the solvers and comparator. |
| `Makefile` | Build, run and compare targets. |
| `run_comparison.sh` | Convenience wrapper: build → run seq → run MPI → compare. |

## Problem specification

Both solvers use the fixed workload from the PDF:

| Parameter | Value |
| --- | --- |
| Grid | `N = 2000` rows, `M = 2000` columns |
| Data type | `double` |
| Iterations | 2,000 synchronous iterations |
| Top boundary (row 0) | `100.0` °C |
| Left / right / bottom boundaries | `20.0` °C, fixed |
| Interior initial value | `20.0 + (h % 1000) / 10.0`, where `h = (i * 73856093) ^ (j * 19349663)` using 64-bit unsigned integers |

Each iteration computes every interior cell from the **previous** grid:

```text
new[i][j] = (old[i-1][j] + old[i+1][j] + old[i][j-1] + old[i][j+1]) / 4.0
```

The boundary values must remain fixed for every iteration.

### Grid distribution

For the required 2-rank case, the plate is split into two contiguous row blocks:

| Rank | Owned global rows | Ghost row needed |
| --- | --- | --- |
| 0 | 0–999 | lower ghost = rank 1's first owned row (global row 1000) |
| 1 | 1000–1999 | upper ghost = rank 0's last owned row (global row 999) |

The edge between the blocks is not a physical boundary. Every iteration each
rank must send its last owned row upward and its first owned row downward,
storing the received rows in ghost rows before computing the next update. The
final implementation must generalise this to `size` ranks.

## Requirements

- A C++17 compiler with MPI support (`mpicxx`, OpenMPI or MPICH on Linux).
- `mpiexec` / `mpirun`.
- GNU Make (optional; the compile commands below work without it).
- On Windows, the MS-MPI SDK/runtime and `mpiexec` used in class.

The build must **not** use `-ffast-math` (GCC) or `/fp:fast` (MSVC): the PDF
requires the update order and floating-point behaviour to be preserved.

## Build

```bash
make all          # builds ./seq, ./dh and ./compare
make toy          # builds ./distributed (the 12 x 12, 4-rank example)
make clean
```

Equivalent manual commands:

```bash
mpicxx -O2 -std=c++17 -Wall -Wextra -pedantic -o seq seq.cxx
mpicxx -O2 -std=c++17 -Wall -Wextra -pedantic -o dh distributed-hosts.cxx
mpicxx -O2 -std=c++17 -Wall -Wextra -pedantic -o compare compare.cxx
```

## Running

### Sequential baseline

```bash
./seq
./seq --dump seq.bin      # also writes the final 2000 x 2000 grid
```

### MPI version

`distributed-hosts.cxx` currently assumes exactly **2 ranks** and refuses other
process counts to avoid overflowing its gather buffer.

```bash
mpiexec -n 2 ./dh
mpiexec -n 2 ./dh --dump mpi.bin
```

On Windows / MS-MPI:

```powershell
mpiexec -n 2 heat_mpi.exe
mpiexec -hosts 2 LAPTOP_A 1 LAPTOP_B 1 -wdir C:\MPI_Project heat_mpi.exe
```

Run the launch service as administrator if needed:

```powershell
sc start MSMPILaunchSvc
```

### Small teaching example

```bash
make toy
mpiexec -n 4 ./toy
```

## Comparing the sequential and MPI results

The PDF (Section 6) defines the correctness metric as:

```text
max_error = max |T_sequential[i,j] - T_MPI[i,j]|
```

Acceptance requires `max_error = 0`. If floating-point differences are
unavoidable, they must be explained and must not exceed `1e-12` °C.

Both solvers accept `--dump <file>`, which writes the final grid after the
timed solver loop. The dump does not change the computation or the reported
solver time. The binary format is documented in `grid_io.h`:

```text
char[8]  "HEATGRID"
int32[2] rows, cols
double[] rows * cols values, row-major
```

### One-command comparison

```bash
make run
# or
./run_comparison.sh
```

The commands are equivalent to:

```bash
./seq --dump seq.bin
mpiexec -n 2 ./dh --dump mpi.bin
./compare seq.bin mpi.bin
```

`compare` prints the maximum absolute error and its location, the maximum error
on interior and boundary cells separately, the number of exactly different
cells, the number of cells above `1e-12`, the mean absolute error and the RMS
error. It exits with `0` for `max_error <= 1e-12` and `1` otherwise, so it can
be used in a script or CI job.

`make run` and `run_comparison.sh` propagate that exit code, so they return
non-zero when the comparison fails. That is intentional: a failing comparison
should be visible to scripts.

### Reference run

On the machine used to prepare this README (`g++ 16`, OpenMPI, `-O2`), the
current `distributed-hosts.cxx` produced:

```text
Grid A (sequential): seq.bin  [2000 x 2000]
Grid B (MPI)       : mpi.bin  [2000 x 2000]

max_error                 = 9.99000000000000057e+01
max_error location        = (i=6, j=1999)
  A[i,j]                  = 2.00000000000000000e+01
  B[i,j]                  = 1.19900000000000006e+02
max_error interior        = 6.89333263255371378e+01
max_error boundary        = 9.99000000000000057e+01
exact mismatches          = 1423068 / 4000000
|error| > 1e-12           = 1326032 / 4000000
non-finite values         = 0
mean |error|              = 1.57109700022419285e+00
RMS error                 = 6.93389581659042431e+00

RESULT: FAIL (max_error > 1e-12; see README for likely causes)
```

The comparison tool is working correctly; the failure is caused by the current
solver implementations. The next section explains why.

## Current correctness status and known issues

`distributed-hosts.cxx` does **not** yet implement the halo (ghost-row) exchange
required by the PDF, and both solvers have a boundary-preservation bug. The
main causes of the nonzero `max_error` are:

1. **No ghost-row exchange between ranks.** The artificial partition boundary at
   global rows 999/1000 is never refreshed. Rank 0's lower ghost row alternates
   between its initial hash value and zero, while rank 1's upper ghost row stays
   at its uninitialised value (`0.0`). This is the largest interior error.
2. **The partition boundary is initialised inconsistently.** Rank 0's ghost row
   is filled from the hash formula, while rank 1's upper ghost row is left at
   zero. The two ranks therefore disagree about the same physical cells.
3. **The right boundary is overwritten by the hash loop.** The loop uses
   `j < M` instead of `j < M - 1`, so column `M - 1` receives a random
   temperature instead of the fixed `20.0`. This causes the `99.9` boundary
   error at `(i=6, j=1999)`.
4. **Boundary layers are not copied into the temporary grid.** In both
   `seq.cxx` and `distributed-hosts.cxx`, only interior cells are written to
   `tempHeatMap`. After each `swap`, the boundary layer alternates between the
   initialised values and zero. After 2,000 (even) iterations the final
   boundary looks initialised again, but every interior update used an
   oscillating boundary layer. Initialising `tempHeatMap = heatMap` once, or
   copying the boundary cells each iteration, fixes this.
5. **The decomposition is hard-coded for 2 ranks.** `rowsPerRank = N / 2`,
   `startRow`, and the `N / 2 + 1` local allocation assume exactly two halves.
   The PDF requires a decomposition that works for a different number of ranks.
6. **Timing does not follow the PDF protocol.** `distributed-hosts.cxx` uses
   `std::chrono` and prints one time per rank. The PDF requires `MPI_Wtime`,
   an `MPI_Barrier` before the timed loop, and `MPI_Reduce(..., MPI_MAX, ...)`
   so the reported distributed runtime is the maximum across ranks.
7. **No hostname evidence.** The PDF requires printing each rank and the
   hostname (for example with `MPI_Get_processor_name`) to prove multi-laptop
   execution.

### Minimal fix checklist

To make the comparison pass:

- Partition rows with `owned = N / size` plus a remainder distribution, and
  allocate two ghost rows (`owned + 2` local rows).
- Convert every local index to a global index with
  `globalRow = rowOffset + (localRow - 1)` and use the **global** indices in the
  hash formula.
- Initialise `tempHeatMap = heatMap` (or copy the boundary cells every
  iteration) so boundaries never change.
- Fix the hash loop to `j < M - 1`, then set column `M - 1` to `20.0`.
- Exchange edge rows every iteration with `MPI_Sendrecv` (or non-blocking
  `MPI_Isend`/`MPI_Irecv` + `MPI_Waitall`):

  ```cpp
  // Send my first owned row upward; receive the down neighbour's first owned
  // row into my lower ghost row.
  MPI_Sendrecv(&heatMap[1][0], M, MPI_DOUBLE, up, 0,
               &heatMap[owned + 1][0], M, MPI_DOUBLE, down, 0,
               MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  // Send my last owned row downward; receive the up neighbour's last owned
  // row into my upper ghost row.
  MPI_Sendrecv(&heatMap[owned][0], M, MPI_DOUBLE, down, 1,
               &heatMap[0][0], M, MPI_DOUBLE, up, 1,
               MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  ```

  Use `MPI_PROC_NULL` for the physical top and bottom neighbours. The two
  different tags keep the two exchanges independent.
- Gather the owned rows with `MPI_Gatherv` (block sizes differ when `N` is not
  divisible by `size`).
- Replace the timing block with `MPI_Barrier` + `MPI_Wtime` and reduce the
  local elapsed time with `MPI_Reduce(..., MPI_MAX, ...)`.
- Print `MPI_Get_processor_name()` together with the rank.
- Re-run `make run` and confirm `RESULT: PASS`.

## Benchmark protocol

The PDF requires the following configurations:

| Case | Physical laptops | Execution | Purpose |
| --- | --- | --- | --- |
| A | 1 | Sequential program, 1 process | Baseline runtime `T_seq` |
| B | 1 | MPI program, 2 ranks on the same laptop | Local parallel execution |
| C | 2 | MPI program, 1 rank per laptop | Mandatory multi-machine run |
| D | 3 | MPI program, 1 rank per laptop | Optional scaling experiment |

Measurement rules from the PDF:

- Use the same workload (`2000 × 2000`, 2,000 iterations) and an optimised
  build (`-O2`) for every case.
- Time only the solver loop. Exclude startup, allocation, result printing, file
  I/O and final gathering, but include per-iteration communication and waiting.
- For MPI, synchronise before the timed loop and report the **maximum** elapsed
  time across ranks (`MPI_Reduce` with `MPI_MAX`), not one rank's time or the
  average.
- Run one untimed warm-up, then at least three timed runs per configuration.
  Report the median and the individual run times.
- Keep laptops plugged in and minimise background activity.

Linux / OpenMPI multi-host example:

```bash
mpiexec --host hostA:1,hostB:1 -n 2 ./dh
```

Windows / MS-MPI example:

```powershell
mpiexec -hosts 2 LAPTOP_A 1 LAPTOP_B 1 -wdir C:\MPI_Project heat_mpi.exe
```

## Notes

- `sum * 0.25` is exactly equal to `sum / 4.0` for IEEE-754 doubles because
  `0.25` is a power of two. For strict conformance with the PDF wording, either
  form is numerically identical, but `/ 4.0` makes the intent explicit.
- `distributed.cxx` is only a 12 × 12 demonstration and requires exactly 4
  ranks. Do not use it for the final benchmark.
- Generated binaries and `*.bin` dumps are ignored by `.gitignore`.
