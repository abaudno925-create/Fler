/* Reads the players straight out of the running game and prints what the
   candidate offsets actually hold. */

#include <cstdio>
#include <string>

#include "../hacks/schema.hpp"
#include "../memory.hpp"
#include "../process.hpp"

static uintptr_t entity_list = 0;

static uintptr_t resolve(uintptr_t index) {
  uintptr_t entry = 0;
  if (!Memory::read(0, (entity_list + 0x10) + 8 * (index >> 9), &entry, 8) || !entry) return 0;
  uintptr_t entity = 0;
  if (!Memory::read(0, entry + 120 * (index & 0x1FF), &entity, 8)) return 0;
  return entity;
}

int main() {
  const pid_t pid = Process::name_to_pid("cs2");
  if (!pid) { printf("cs2 is not running\n"); return 1; }

  const uintptr_t engine = Memory::module_base_address(pid, "libengine2.so");
  const uintptr_t resource = Memory::interface_offset(pid, engine, "GameResourceServiceClientV0");
  Memory::read(pid, resource + 0x50, &entity_list, 8);
  printf("entity list %p\n", (void *)entity_list);

  Schema::dump(pid);
  printf("schema: %d classes, %s\n", Schema::class_count(), Schema::status().c_str());

  const int pawn_handle = Schema::offset("CBasePlayerController", "m_hPawn");
  printf("m_hPawn = 0x%x\n", pawn_handle);

  uintptr_t entry = 0;
  Memory::read(pid, entity_list + 0x10, &entry, 8);

  for (unsigned long i = 1; i <= 16; ++i) {
    uintptr_t controller = 0;
    Memory::read(pid, entry + 120 * (i & 0x1FF), &controller, 8);
    if (!controller) continue;

    uint32_t handle = 0;
    Memory::read(pid, controller + pawn_handle, &handle, 4);
    const uintptr_t pawn = resolve(handle & 0x7FFF);
    if (!pawn) { printf("%2lu controller %p handle 0x%x -> no pawn\n", i, (void*)controller, handle); continue; }

    int health_a = 0, health_b = 0;
    Memory::read(pid, pawn + 0x5B0, &health_a, 4);
    Memory::read(pid, pawn + 0x4BC, &health_b, 4);

    unsigned char team_a = 0, team_b = 0;
    Memory::read(pid, pawn + 0x557, &team_a, 1);
    Memory::read(pid, pawn + 0x55B, &team_b, 1);

    float sim = 0.0f;
    Memory::read(pid, pawn + 0x528, &sim, 4);

    float origin[3] = {0, 0, 0};
    Memory::read(pid, pawn + 0xD40, origin, 12);

    uintptr_t name_ptr = 0;
    Memory::read(pid, controller + 0x9F8, &name_ptr, 8);
    std::string name;
    if (name_ptr > 0x1000) name = Memory::read_string(pid, name_ptr);

    printf("%2lu ctrl=%p pawn=%p health(0x5B0)=%d health(0x4BC)=%d team(0x557)=%d team(0x55B)=%d "
           "sim=%.4f origin=(%.1f %.1f %.1f) name=[%s]\n",
           i, (void *)controller, (void *)pawn, health_a, health_b, team_a, team_b, sim,
           origin[0], origin[1], origin[2], name.c_str());
  }
  return 0;
}
