/* Finds the entity list layout and the health field of the running game by
   brute force: the right layout is the one that yields many entities with
   the same int32 = 100 (the health of a living player). */

#include <cstdio>
#include <map>

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

  struct Variant {
    const char *name;
    uintptr_t base;
    bool two_level;
  };

  const Variant variants[] = {
      {"A list+0x10 flat", inner, false},
      {"B list flat", list, false},
      {"C list two level", list, true},
      {"D list+0x10 two level", inner, true},
  };

  for (const Variant &variant : variants) {
    std::map<uintptr_t, int> health_hits;

    int found = 0;
    for (unsigned long i = 1; i <= 96; ++i) {
      uintptr_t entity = 0;
      if (variant.two_level) {
        uintptr_t chunk = 0;
        Memory::read(pid, variant.base + 8 * (i >> 9), &chunk, 8);
        if (!heap_ptr(chunk)) continue;
        Memory::read(pid, chunk + 0x80 * (i & 0x1FF), &entity, 8);
      } else {
        Memory::read(pid, variant.base + 0x80 * i, &entity, 8);
      }
      if (!heap_ptr(entity)) continue;
      ++found;

      for (uintptr_t off = 0; off < 0x1000; off += 4) {
        int v = 0;
        if (!Memory::read(pid, entity + off, &v, 4)) break;
        if (v == 100) ++health_hits[off];
      }
    }

    printf("%-26s entities=%3d  ", variant.name, found);
    int best = 0;
    uintptr_t best_off = 0;
    for (const auto &hit : health_hits) {
      if (hit.second > best) {
        best = hit.second;
        best_off = hit.first;
      }
    }
    if (best > 1) {
      printf("health at +0x%lx in %d entities\n", best_off, best);
    } else {
      printf("no common health field\n");
    }
  }

  return 0;
}
