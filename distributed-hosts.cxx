#include "grid_io.h"


#include <cstdint>
#include <iostream>
#include <mpi.h>
#include <string>
#include <vector>

#define N 2000
#define M 2000
using namespace std;

int main(int argc, char **argv) {

  vector<vector<double>> heatMap(N / 2 + 1, vector<double>(M));
  vector<vector<double>> tempHeatMap(N / 2 + 1, vector<double>(M));

  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  string dumpPath;
  for (int i = 1; i < argc; ++i) {
    const string arg = argv[i];
    if (arg == "--dump" && i + 1 < argc) {
      dumpPath = argv[++i];
    } else {
      if (rank == 0)
        cerr << "usage: mpiexec -n 2 " << argv[0] << " [--dump <grid.bin>]\n";
      MPI_Finalize();
      return 2;
    }
  }

  // This decomposition is hard-coded for two halves of the plate. Refuse other
  // process counts instead of overflowing the rank-0 gather buffer.
  if (size != 2) {
    if (rank == 0)
      cerr << "distributed-hosts requires exactly 2 MPI ranks (got " << size
           << ").\n";
    MPI_Finalize();
    return 2;
  }

  char hostname[MPI_MAX_PROCESSOR_NAME];
  int hostnameLen;
  MPI_Get_processor_name(hostname, &hostnameLen);
  cout << "--- Started the Process from rank: " << rank
       << " on host: " << hostname << " ---\n";

  // initialization phase
  for (unsigned int i = 1; i < N / 2; ++i) {
    heatMap[i][0] = 20.0;
    heatMap[i][M - 1] = 20.0;
    
    tempHeatMap[i][0] = 20.0;
    tempHeatMap[i][M - 1] = 20.0;
  }

  if (rank == 0) {
    for (unsigned int i = 0; i < M; ++i) {
      heatMap[0][i] = 100.0;
      tempHeatMap[0][i] = 100.0;
    }
  }

  uint64_t h;
  for (unsigned int i = 1; i <= N / 2; ++i) {
    for (unsigned int j = 1; j < M - 1; ++j) {
      h = ((uint64_t)(i + rank * 999) * 73856093ULL) ^
          ((uint64_t)(j) * 19349663ULL);
      heatMap[i][j] = 20.0 + (h % 1000) / 10.0;
    }
  }

  if (rank) {
    for (unsigned int i = 0; i < M; ++i) {
      heatMap[N / 2][i] = 20.0;
      tempHeatMap[N / 2][i] = 20.0;
    }
  }

  double sum;
  MPI_Barrier(MPI_COMM_WORLD);
  double start = MPI_Wtime();
  for (unsigned int k = 0; k < 2000; ++k) {
    if (rank == 0) {
      // Send last owned row down to rank 1, receive rank 1's first owned row into ghost
      MPI_Sendrecv(&heatMap[N / 2 - 1][0], M, MPI_DOUBLE, 1, 0,
                   &heatMap[N / 2][0],     M, MPI_DOUBLE, 1, 1,
                   MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } else {
      // Send first owned row up to rank 0, receive rank 0's last owned row into ghost
      MPI_Sendrecv(&heatMap[1][0],  M, MPI_DOUBLE, 0, 1,
                   &heatMap[0][0],  M, MPI_DOUBLE, 0, 0,
                   MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    for (unsigned int i = 1; i < N / 2; ++i) {
      for (unsigned int j = 1; j < M - 1; ++j) {
        sum = heatMap[i - 1][j] + heatMap[i + 1][j] + heatMap[i][j - 1] +
              heatMap[i][j + 1];
        // used * 0.25 for free optimization
        tempHeatMap[i][j] = sum * 0.25;
      }
    }
    std::swap(heatMap, tempHeatMap);
  }
  double localElapsed = MPI_Wtime() - start;
  double maxElapsed;
  MPI_Reduce(&localElapsed, &maxElapsed, 1, MPI_DOUBLE, MPI_MAX, 0,
             MPI_COMM_WORLD);
  if (rank == 0)
    cout << "Execution time (max across ranks): " << maxElapsed << " seconds\n";

  // Gather final grid on rank 0
  const int rowsPerRank = N / 2;
  const int elementsPerRank = rowsPerRank * M;

  // Each rank prepares only its owned rows
  vector<double> localData(elementsPerRank);

  int startRow = (rank == 0) ? 0 : 1;

  for (int i = 0; i < rowsPerRank; ++i) {
    for (int j = 0; j < M; ++j) {
      localData[i * M + j] = heatMap[startRow + i][j];
    }
  }

  // Rank 0 allocates space for the complete grid
  vector<double> finalGrid;

  if (rank == 0) {
    finalGrid.resize(N * M);
  }

  // Collect data from all ranks
  MPI_Gather(localData.data(), elementsPerRank, MPI_DOUBLE,
             rank == 0 ? finalGrid.data() : nullptr, elementsPerRank,
             MPI_DOUBLE, 0, MPI_COMM_WORLD);

  if (!rank) {
    cout << finalGrid.size() << " from rank 0\n";

    if (!dumpPath.empty()) {
      if (!heat::writeGrid(dumpPath, finalGrid.data(), finalGrid.size(), N, M)) {
        cerr << "failed to write final grid to " << dumpPath << "\n";
        MPI_Finalize();
        return 3;
      }
      cout << "Final grid written to " << dumpPath << "\n";
    }
  }

  MPI_Finalize();
  return 0;
}
