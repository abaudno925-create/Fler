/* Brute force: which layout and which offset hold the health of the players?
   The right answer is the one where many entities share the same small
   value at the same offset. */

#include <cstdio>
#include <map>
#include <vector>

#include "memory.hpp"
#include "process.hpp"

static bool heap_ptr(uintptr_t v) { return v > 0x100000000ULL && v < 0x700000000000ULL; }

int main() {
  const pid_t pid = Process::name_to_pid("cs2");
  const uintptr_t engine = Memory::module_base_address(pid, "libengine2.so");
  const uintptr_t resource = Memory::interface_offset(pid, engine, "GameResourceServiceClientV0");
  uintptr_t list = 0;
  Memory::read(pid, resource + 0x50, &list, 8);

  uintptr_t inner = 0;
  Memory::read(pid, list + 0x10, &inner, 8);

  for (uintptr_t chunk_off : {0x00u, 0x10u}) {
    for (uintptr_t stride : {0x78u, 0x80u}) {
      for (int mode = 0; mode < 2; ++mode) {
        uintptr_t chunk = 0;
        Memory::read(pid, list + chunk_off, &chunk, 8);
        if (!heap_ptr(chunk)) continue;

        std::vector<uintptr_t> entities;
        for (unsigned long i = 1; i <= 64; ++i) {
          uintptr_t e = 0;
          if (mode == 0) {
            e = chunk + stride * i;              /* identities in place */
          } else {
            Memory::read(pid, chunk + stride * i, &e, 8); /* pointers */
            if (!heap_ptr(e)) continue;
          }
          entities.push_back(e);
        }
        if (entities.size() < 3) continue;

        std::map<uintptr_t, std::map<int, int>> values;
        for (uintptr_t e : entities) {
          for (uintptr_t off = 0; off < 0x1400; off += 4) {
            int v = 0;
            if (!Memory::read(pid, e + off, &v, 4)) break;
            if (v >= 1 && v <= 100) values[off][v]++;
          }
        }

        for (const auto &entry : values) {
          for (const auto &value : entry.second) {
            if (value.second >= 3) {
              printf("chunk+0x%lx stride 0x%lx mode %d: offset 0x%lx value %d in %d/%zu entities\n",
                     chunk_off, stride, mode, entry.first, value.first, value.second, entities.size());
            }
          }
        }
      }
    }
  }
  return 0;
}
