#include <cstdint>
#include <iostream>
#include <ostream>
#include <vector>

using namespace std;

#define N 5
#define M 5

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

int main() {
  vector<vector<double>> heatMap = initializeGrid();
  for (auto row : heatMap) {
    for (auto col : row) {
      cout << col << " ";
    }
    cout << endl;
  }
  cout << "Hello Distributed Shit";
}
