#include <cstdio>

#include "hacks/player_info.hpp"
#include "hacks/schema.hpp"
#include "memory.hpp"
#include "process.hpp"

int main() {
  const pid_t pid = Process::name_to_pid("cs2");
  if (!pid) { printf("cs2 is not running\n"); return 1; }

  const uintptr_t engine = Memory::module_base_address(pid, "libengine2.so");
  const uintptr_t resource = Memory::interface_offset(pid, engine, "GameResourceServiceClientV0");
  Memory::read(pid, resource + 0x50, &PlayerInfo::entity_list, 8);
  printf("entity list %p\n", (void *)PlayerInfo::entity_list);

  if (Schema::dump(pid)) printf("schema: %d classes\n", Schema::class_count());
  else printf("schema failed: %s\n", Schema::status().c_str());

  players(pid);

  printf("local player index: %d\n", PlayerInfo::i_local_player);
  for (int i = 0; i < 12; ++i) {
    const PlayerInfo::Player &p = PlayerInfo::l_players[i];
    if (p.health <= 0) continue;
    printf("  [%d] name=[%s] hp=%d team=%d origin=(%.1f %.1f %.1f) weapon=[%s] bone6=(%.2f %.2f %.2f)\n",
           i, p.name.c_str(), p.health, p.team, p.location.x, p.location.y, p.location.z,
           p.weapon_name.c_str(), p.bone_matrix[6].x, p.bone_matrix[6].y, p.bone_matrix[6].z);
  }
  printf("local weapon entity: %p\n", (void *)PlayerInfo::local_weapon_address);
  return 0;
}
