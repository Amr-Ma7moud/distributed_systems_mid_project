#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <mpi.h>
#include <vector>

constexpr int N = 12;
constexpr int OWNED = 3;
constexpr int LOCAL_ROWS = OWNED + 2;
constexpr double TOL = 0.01;
constexpr int MAX_ITERS = 100;

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (size != 4) {
    if (rank == 0)
      std::cerr << "Run with exactly 4 MPI processes.\n";
    MPI_Finalize();
    return 1;
  }

  // local row 0 and row 4 are ghost rows; rows 1..3 are owned rows.
  std::vector<double> x(LOCAL_ROWS * N, 0.0);
  std::vector<double> xnew(LOCAL_ROWS * N, 0.0);
  auto at = [&](std::vector<double> &a, int row, int col) -> double & {
    return a[row * N + col];
  };

  // Example boundary conditions: top edge = 100; other perimeter edges = 0.
  // All interior cells start at 0. The lab slides do not specify exact values.
  for (int i = 1; i <= OWNED; ++i) {
    int globalRow = rank * OWNED + (i - 1);
    for (int j = 0; j < N; ++j) {
      if (globalRow == 0)
        at(x, i, j) = 100.0;
      else
        at(x, i, j) = 0.0;
      if (j == 0 || j == N - 1 || globalRow == N - 1) {
        at(x, i, j) = 0.0;
      }
    }
  }
  // Keep the top-left and top-right corners at 0, consistent with side
  // boundaries.
  for (int j = 0; j < N; ++j) {
    if (j == 0 || j == N - 1)
      at(x, 1, j) = 0.0;
  }
  xnew = x;

  int up = (rank == 0) ? MPI_PROC_NULL : rank - 1;
  int down = (rank == size - 1) ? MPI_PROC_NULL : rank + 1;

  int iterations = 0;
  double globalMaxChange = 0.0;

  do {
    // Exchange current boundary rows into ghost rows.
    MPI_Sendrecv(&at(x, 1, 0), N, MPI_DOUBLE, up, 0, &at(x, OWNED + 1, 0), N,
                 MPI_DOUBLE, down, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    MPI_Sendrecv(&at(x, OWNED, 0), N, MPI_DOUBLE, down, 1, &at(x, 0, 0), N,
                 MPI_DOUBLE, up, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    double localMaxChange = 0.0;

    // Update only global interior cells. Physical boundaries remain fixed.
    for (int i = 1; i <= OWNED; ++i) {
      int globalRow = rank * OWNED + (i - 1);
      for (int j = 0; j < N; ++j) {
        at(xnew, i, j) = at(x, i, j); // preserve boundaries by default
        if (globalRow > 0 && globalRow < N - 1 && j > 0 && j < N - 1) {
          double value = (at(x, i - 1, j) + at(x, i + 1, j) + at(x, i, j - 1) +
                          at(x, i, j + 1)) /
                         4.0;
          at(xnew, i, j) = value;
          localMaxChange =
              std::max(localMaxChange, std::abs(value - at(x, i, j)));
        }
      }
    }

    MPI_Allreduce(&localMaxChange, &globalMaxChange, 1, MPI_DOUBLE, MPI_MAX,
                  MPI_COMM_WORLD);

    // Commit this iteration's values.
    for (int i = 1; i <= OWNED; ++i) {
      for (int j = 0; j < N; ++j)
        at(x, i, j) = at(xnew, i, j);
    }

    ++iterations;
  } while (globalMaxChange >= TOL && iterations < MAX_ITERS);

  // Gather counts first, as requested, then gather each rank's owned rows.
  int localCount = OWNED * N;
  std::vector<int> counts, displs;
  if (rank == 0) {
    counts.resize(size);
    displs.resize(size);
  }

  MPI_Gather(&localCount, 1, MPI_INT, rank == 0 ? counts.data() : nullptr, 1,
             MPI_INT, 0, MPI_COMM_WORLD);

  std::vector<double> globalGrid;
  if (rank == 0) {
    globalGrid.resize(N * N);
    int offset = 0;
    for (int r = 0; r < size; ++r) {
      displs[r] = offset;
      offset += counts[r];
    }
  }

  // Rows 1..3 are contiguous: send exactly 36 owned values, no ghosts.
  MPI_Gatherv(&at(x, 1, 0), localCount, MPI_DOUBLE,
              rank == 0 ? globalGrid.data() : nullptr,
              rank == 0 ? counts.data() : nullptr,
              rank == 0 ? displs.data() : nullptr, MPI_DOUBLE, 0,
              MPI_COMM_WORLD);

  if (rank == 0) {
    std::cout << "Iterations: " << iterations
              << ", final global max change: " << globalMaxChange << "\n";
    std::cout << "Final 12 x 12 grid:\n";
    std::cout << std::fixed << std::setprecision(2);
    for (int i = 0; i < N; ++i) {
      for (int j = 0; j < N; ++j) {
        std::cout << std::setw(7) << globalGrid[i * N + j];
      }
      std::cout << '\n';
    }
  }

  MPI_Finalize();
  return 0;
}
