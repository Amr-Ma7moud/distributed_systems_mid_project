#include "grid_io.h"

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

void usage(const char *program) {
  std::cerr << "usage: " << program
            << " <sequential-grid.bin> <mpi-grid.bin>\n";
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    usage(argv[0]);
    return 2;
  }

  std::vector<double> seq;
  std::vector<double> mpi;
  int seqRows = 0, seqCols = 0;
  int mpiRows = 0, mpiCols = 0;
  std::string error;

  if (!heat::readGrid(argv[1], seq, seqRows, seqCols, error)) {
    std::cerr << "error: " << error << "\n";
    return 2;
  }
  if (!heat::readGrid(argv[2], mpi, mpiRows, mpiCols, error)) {
    std::cerr << "error: " << error << "\n";
    return 2;
  }

  if (seqRows != mpiRows || seqCols != mpiCols) {
    std::cerr << "error: grid dimensions differ: " << seqRows << "x" << seqCols
              << " vs " << mpiRows << "x" << mpiCols << "\n";
    return 2;
  }
  if (seq.size() != mpi.size()) {
    std::cerr << "error: grid element counts differ\n";
    return 2;
  }

  constexpr double kTolerance = 1e-12;

  double maxError = 0.0;
  double maxErrorInterior = 0.0;
  double maxErrorBoundary = 0.0;
  std::size_t maxIndex = 0;
  std::size_t exactMismatches = 0;
  std::size_t aboveTolerance = 0;
  std::size_t nonFinite = 0;
  long double sumAbs = 0.0L;
  long double sumSquared = 0.0L;

  for (std::size_t k = 0; k < seq.size(); ++k) {
    const double a = seq[k];
    const double b = mpi[k];

    if (!std::isfinite(a) || !std::isfinite(b)) {
      ++nonFinite;
      continue;
    }

    const double diff = std::fabs(a - b);
    if (a != b) {
      ++exactMismatches;
    }
    if (diff > kTolerance) {
      ++aboveTolerance;
    }

    if (diff > maxError) {
      maxError = diff;
      maxIndex = k;
    }

    const int row = static_cast<int>(k / static_cast<std::size_t>(seqCols));
    const int col = static_cast<int>(k % static_cast<std::size_t>(seqCols));
    const bool boundary =
        row == 0 || row == seqRows - 1 || col == 0 || col == seqCols - 1;
    if (boundary) {
      if (diff > maxErrorBoundary) {
        maxErrorBoundary = diff;
      }
    } else if (diff > maxErrorInterior) {
      maxErrorInterior = diff;
    }

    sumAbs += static_cast<long double>(diff);
    sumSquared += static_cast<long double>(diff) * diff;
  }

  const std::size_t count = seq.size();
  const double meanAbs = static_cast<double>(sumAbs / count);
  const double rms = std::sqrt(static_cast<double>(sumSquared / count));
  const int maxRow =
      static_cast<int>(maxIndex / static_cast<std::size_t>(seqCols));
  const int maxCol =
      static_cast<int>(maxIndex % static_cast<std::size_t>(seqCols));

  std::cout << std::scientific << std::setprecision(17);
  std::cout << "Grid A (sequential): " << argv[1] << "  [" << seqRows << " x "
            << seqCols << "]\n";
  std::cout << "Grid B (MPI)       : " << argv[2] << "  [" << mpiRows << " x "
            << mpiCols << "]\n";
  std::cout << "\n";
  std::cout << "max_error                 = " << maxError << "\n";
  std::cout << "max_error location        = (i=" << maxRow << ", j=" << maxCol
            << ")\n";
  std::cout << "  A[i,j]                  = " << seq[maxIndex] << "\n";
  std::cout << "  B[i,j]                  = " << mpi[maxIndex] << "\n";
  std::cout << "max_error interior        = " << maxErrorInterior << "\n";
  std::cout << "max_error boundary        = " << maxErrorBoundary << "\n";
  std::cout << "exact mismatches          = " << exactMismatches << " / "
            << count << "\n";
  std::cout << "|error| > 1e-12           = " << aboveTolerance << " / "
            << count << "\n";
  std::cout << "non-finite values         = " << nonFinite << "\n";
  std::cout << "mean |error|              = " << meanAbs << "\n";
  std::cout << "RMS error                 = " << rms << "\n";

  const bool pass = nonFinite == 0 && maxError <= kTolerance;
  std::cout << "\nRESULT: " << (pass ? "PASS" : "FAIL");
  if (pass) {
    std::cout << " (max_error <= 1e-12)\n";
  } else {
    std::cout << " (max_error > 1e-12; see README for likely causes)\n";
  }

  return pass ? 0 : 1;
}
