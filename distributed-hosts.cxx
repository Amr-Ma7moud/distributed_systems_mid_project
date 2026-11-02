#include <chrono>
#include <cstdint>
#include <iostream>
#include <mpi.h>
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
  cout << "--- Started the Process from rank: " << rank << " ---\n";

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
  auto start = chrono::high_resolution_clock::now();
  for (unsigned int k = 0; k < 2000; ++k) {
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
  auto end = chrono::high_resolution_clock::now();
  chrono::duration<double> elapsed = end - start;
  cout << "Execution time: " << elapsed.count() << " seconds for rank: " << rank
       << endl;

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

  if (!rank)
    cout << finalGrid.size() << " from rank 0";

  MPI_Finalize();
  return 0;
}
