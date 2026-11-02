#include "grid_io.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

#define N 2000
#define M 2000

std::vector<std::vector<double>> grid(N, std::vector<double>(M, 0.0));

vector<vector<double>> initializeGrid() {
  uint64_t h;
  for (unsigned int i = 1; i < N - 1; ++i) {
    for (unsigned int j = 1; j < M - 1; ++j) {
      h = ((uint64_t)i * 73856093ULL) ^ ((uint64_t)j * 19349663ULL);
      grid[i][j] = 20.0 + (h % 1000) / 10.0;
    }
  }

  for (unsigned int i = 1; i < N; ++i) {
    grid[i][0] = 20;
    grid[i][M - 1] = 20;
  }

  for (unsigned int i = 0; i < M; ++i) {
    grid[0][i] = 100;
    grid[N - 1][i] = 20;
  }
  return grid;
}

int main(int argc, char **argv) {
  string dumpPath;
  for (int i = 1; i < argc; ++i) {
    const string arg = argv[i];
    if (arg == "--dump" && i + 1 < argc) {
      dumpPath = argv[++i];
    } else {
      cerr << "usage: " << argv[0] << " [--dump <grid.bin>]\n";
      return 2;
    }
  }

  auto initializationStart = chrono::high_resolution_clock::now();
  vector<vector<double>> heatMap = initializeGrid();
  vector<vector<double>> tempHeatMap = heatMap;
  auto initializationEnd = chrono::high_resolution_clock::now();
  chrono::duration<double> initializationElapsed =
      initializationEnd - initializationStart;
  cout << "initialization time: " << initializationElapsed.count()
       << " seconds\n";

  double sum;
  auto start = chrono::high_resolution_clock::now();
  for (unsigned int k = 0; k < 2000; ++k) {
    for (unsigned int i = 1; i < N - 1; ++i) {
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
  cout << "Execution time: " << elapsed.count() << " seconds\n";

  if (!dumpPath.empty()) {
    if (!heat::writeGridRows(dumpPath, heatMap, N, M)) {
      cerr << "failed to write final grid to " << dumpPath << "\n";
      return 3;
    }
    cout << "Final grid written to " << dumpPath << "\n";
  }

  return 0;
}
