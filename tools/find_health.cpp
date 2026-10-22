/* Watches the players and reports the offsets where the health lives: the
   value starts at 100 and drops when the player takes damage. */

#include <cstdio>
#include <map>
#include <vector>

#include "hacks/offsets.hpp"
#include "memory.hpp"
#include "process.hpp"

int main() {
  const pid_t pid = Process::name_to_pid("cs2");
  const uintptr_t engine = Memory::module_base_address(pid, "libengine2.so");
  const uintptr_t resource = Memory::interface_offset(pid, engine, "GameResourceServiceClientV0");
  uintptr_t list = 0;
  Memory::read(pid, resource + 0x50, &list, 8);
  uintptr_t array = 0;
  Memory::read(pid, list + 0x10, &array, 8);

  std::vector<uintptr_t> players;
  for (unsigned long i = 0; i < 512; ++i) {
    uintptr_t e = 0;
    Memory::read(pid, array + 0x80 * i, &e, 8);
    if (e < 0x100000000ULL || e > 0x700000000000ULL) continue;
    int team = 0;
    Memory::read(pid, e + Offsets::kTeam, &team, 4);
    if (team < 1 || team > 3) continue;
    uintptr_t scene = 0;
    Memory::read(pid, e + Offsets::kGameScene, &scene, 8);
    if (scene < 0x100000000ULL || scene > 0x700000000000ULL) continue;
    float o[3] = {0, 0, 0};
    Memory::read(pid, scene + Offsets::kSceneOrigin, o, 12);
    if (o[0] == 0 && o[1] == 0) continue;
    players.push_back(e);
  }

  printf("%zu players\n", players.size());
  if (players.empty()) return 1;

  const size_t count = players.size();
  std::vector<std::map<uintptr_t, int>> snapshot(count);
  for (size_t p = 0; p < count; ++p) {
    for (uintptr_t off = 0; off < 0x1800; off += 4) {
      int v = 0;
      if (!Memory::read(pid, players[p] + off, &v, 4)) break;
      if (v > 0) snapshot[p][off] = v;
    }
  }

  for (int round = 0; round < 60; ++round) {
    usleep(100000);
    for (size_t p = 0; p < count; ++p) {
      for (const auto &entry : snapshot[p]) {
        int v = 0;
        if (!Memory::read(pid, players[p] + entry.first, &v, 4)) continue;
        if (entry.second == 100 && v < 100 && v >= 0) {
          printf("player %zu entity %p: offset 0x%lx health %d -> %d\n", p, (void *)players[p],
                 entry.first, entry.second, v);
        }
      }
    }
    if (!snapshot.empty()) break;
  }
  return 0;
}
