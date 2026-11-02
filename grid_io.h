#ifndef HEAT_GRID_IO_H
#define HEAT_GRID_IO_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// Shared binary grid format used by seq.cxx, distributed-hosts.cxx and
// compare.cxx.
//
// Layout:
//   char[8]  magic  = "HEATGRID"
//   int32[2] dims   = { rows, cols } (native endianness)
//   double[] values = rows * cols values, row-major
namespace heat {

inline bool writeGrid(const std::string &path, const double *data,
                      std::size_t count, int rows, int cols) {
  if (data == nullptr || rows <= 0 || cols <= 0 ||
      count != static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols)) {
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }

  out.write("HEATGRID", 8);
  const std::int32_t dims[2] = {rows, cols};
  out.write(reinterpret_cast<const char *>(dims), sizeof(dims));
  out.write(reinterpret_cast<const char *>(data),
            static_cast<std::streamsize>(sizeof(double) * count));
  return out.good();
}

inline bool writeGridRows(const std::string &path,
                          const std::vector<std::vector<double>> &rows,
                          int n, int m) {
  if (n <= 0 || m <= 0 || static_cast<int>(rows.size()) != n) {
    return false;
  }

  std::vector<double> flat(static_cast<std::size_t>(n) *
                           static_cast<std::size_t>(m));
  for (int i = 0; i < n; ++i) {
    if (static_cast<int>(rows[i].size()) != m) {
      return false;
    }
    std::memcpy(&flat[static_cast<std::size_t>(i) * static_cast<std::size_t>(m)],
                rows[i].data(), sizeof(double) * static_cast<std::size_t>(m));
  }

  return writeGrid(path, flat.data(), flat.size(), n, m);
}

inline bool readGrid(const std::string &path, std::vector<double> &data,
                     int &rows, int &cols, std::string &error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "cannot open " + path;
    return false;
  }

  char magic[8];
  in.read(magic, sizeof(magic));
  if (!in || std::memcmp(magic, "HEATGRID", 8) != 0) {
    error = path + ": not a HEATGRID file";
    return false;
  }

  std::int32_t dims[2];
  in.read(reinterpret_cast<char *>(dims), sizeof(dims));
  if (!in || dims[0] <= 0 || dims[1] <= 0) {
    error = path + ": invalid grid dimensions";
    return false;
  }

  rows = static_cast<int>(dims[0]);
  cols = static_cast<int>(dims[1]);
  const std::size_t count = static_cast<std::size_t>(rows) *
                            static_cast<std::size_t>(cols);
  data.resize(count);
  in.read(reinterpret_cast<char *>(data.data()),
          static_cast<std::streamsize>(sizeof(double) * count));
  if (!in) {
    error = path + ": truncated grid payload";
    return false;
  }

  return true;
}

} // namespace heat

#endif // HEAT_GRID_IO_H
