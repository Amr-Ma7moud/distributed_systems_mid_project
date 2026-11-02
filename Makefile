MPICXX ?= mpicxx
MPIEXEC ?= mpiexec
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -pedantic
MPIEXEC_FLAGS ?=

.PHONY: all seq dh compare toy run clean

all: seq dh compare

seq: seq.cxx grid_io.h
	$(MPICXX) $(CXXFLAGS) -o $@ seq.cxx

dh: distributed-hosts.cxx grid_io.h
	$(MPICXX) $(CXXFLAGS) -o $@ distributed-hosts.cxx

compare: compare.cxx grid_io.h
	$(MPICXX) $(CXXFLAGS) -o $@ compare.cxx

# Small 12 x 12 teaching example (requires exactly 4 ranks).
toy: distributed.cxx
	$(MPICXX) $(CXXFLAGS) -o $@ distributed.cxx

# Build, run both solvers, and compare their final grids.
run: all
	./seq --dump seq.bin
	$(MPIEXEC) $(MPIEXEC_FLAGS) -n 2 ./dh --dump mpi.bin
	./compare seq.bin mpi.bin

clean:
	rm -f seq dh compare toy seq.bin mpi.bin
