/* Finds the world to screen matrix in the client by scanning for the shape of
   a view matrix: a unit forward vector in row two and the same vector in row
   three. */

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "memory.hpp"
#include "process.hpp"

int main(int argc, char **argv) {
  const pid_t pid = Process::name_to_pid("cs2");
  const char *module = argc > 1 ? argv[1] : "libclient.so";

  std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");
  std::string line;

  while (std::getline(maps, line)) {
    if (line.find(module) == std::string::npos) continue;
    if (line.find('w') == std::string::npos) continue;

    uintptr_t start = 0, end = 0;
    if (sscanf(line.c_str(), "%lx-%lx", &start, &end) != 2) continue;
    if (end <= start || end - start > 256 * 1024 * 1024) continue;

    const int fd = open(("/proc/" + std::to_string(pid) + "/mem").c_str(), O_RDONLY);
    if (fd < 0) continue;
    std::vector<unsigned char> buffer(end - start);
    const ssize_t got = pread(fd, buffer.data(), buffer.size(), (off_t)start);
    close(fd);
    if (got <= 0) continue;

    /* the view matrix is usually behind a pointer, look at both */
    std::vector<uintptr_t> candidates;
    for (size_t offset = 0; offset + 8 <= buffer.size(); offset += 8) {
      uintptr_t value = 0;
      memcpy(&value, &buffer[offset], 8);
      if (value > 0x100000000ULL && value < 0x700000000000ULL) candidates.push_back(value);
    }

    for (uintptr_t candidate : candidates) {
      float m[16] = {0};
      if (!Memory::read(pid, candidate, m, sizeof(m))) continue;

      const float fx = m[12], fy = m[13], fz = m[14];
      if (!std::isfinite(fx) || !std::isfinite(fy) || !std::isfinite(fz)) continue;
      const float len = sqrtf(fx * fx + fy * fy + fz * fz);
      if (len < 0.85f || len > 1.15f) continue;

      /* the view distance sits in the last slot */
      if (!std::isfinite(m[15])) continue;
      if (!std::isfinite(m[15]) || fabsf(m[15]) > 1e6f) continue;
      if (!std::isfinite(m[3]) || !std::isfinite(m[7])) continue;

      printf("view matrix at %p\n", (void *)candidate);
      for (int r = 0; r < 4; ++r)
        printf("  row%d: %12.5f %12.5f %12.5f %12.5f\n", r, m[r * 4], m[r * 4 + 1], m[r * 4 + 2], m[r * 4 + 3]);
      printf("  scale %f %f\n", fabsf(m[0]) + fabsf(m[1]), fabsf(m[4]) + fabsf(m[5]));
      return 0;
    }
  }

  printf("nothing found\n");
  return 1;
}
