#include <cstdio>
#include "hacks/schema.hpp"
#include "memory.hpp"
#include "process.hpp"

static pid_t pid;

static uintptr_t resolve(uintptr_t list, uintptr_t index, uintptr_t stride) {
  uintptr_t array = 0;
  Memory::read(pid, list + 0x10, &array, 8);
  uintptr_t entity = 0;
  Memory::read(pid, array + stride * (index & 0x1FF), &entity, 8);
  return entity;
}

int main() {
  pid = Process::name_to_pid("cs2");
  const uintptr_t engine = Memory::module_base_address(pid, "libengine2.so");
  const uintptr_t resource = Memory::interface_offset(pid, engine, "GameResourceServiceClientV0");
  uintptr_t list = 0;
  Memory::read(pid, resource + 0x50, &list, 8);
  Schema::dump(pid);
  const int pawn_handle = Schema::offset("CBasePlayerController", "m_hPawn");
  const int health = Schema::offset("C_CSPlayerPawn", "m_iHealth");
  printf("list=%p m_hPawn=0x%x health=0x%x\n", (void*)list, pawn_handle, health);

  for (uintptr_t stride : {120u, 128u, 0x78u}) {
    printf("--- stride %u ---\n", stride);
    for (unsigned long i = 0; i < 8; ++i) {
      uintptr_t controller = resolve(list, i, stride);
      if (!controller) { printf("  [%lu] null\n", i); continue; }
      uint32_t handle = 0;
      Memory::read(pid, controller + pawn_handle, &handle, 4);
      uintptr_t pawn = 0;
      if (handle & 0x7FFF) {
        uintptr_t entry = 0;
        Memory::read(pid, (list + 0x10) + 8 * ((handle & 0x7FFF) >> 9), &entry, 8);
        if (entry) Memory::read(pid, entry + stride * (handle & 0x1FF), &pawn, 8);
      }
      int hp = -1;
      if (pawn) Memory::read(pid, pawn + health, &hp, 4);
      printf("  [%lu] controller=%p handle=0x%x pawn=%p health=%d\n", i, (void*)controller, handle,
             (void*)pawn, hp);
    }
  }
  return 0;
}
