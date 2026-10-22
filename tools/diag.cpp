/* Walks the same resolution steps as main.cpp and prints where each one
   lands. Run it while CS2 is open:

     g++ -O2 -I. tools/diag.cpp -o /tmp/diag && /tmp/diag
*/

#include <cstdio>

#include "../memory.hpp"
#include "../process.hpp"

int main() {
  const pid_t game_pid = Process::name_to_pid("cs2");
  if (!game_pid) {
    printf("no cs2 process\n");
    return 1;
  }
  printf("pid            %d\n", game_pid);

  const uintptr_t client_address = Memory::module_base_address(game_pid, "libclient.so");
  printf("libclient.so   %p\n", (void *)client_address);

  const uintptr_t engine_address = Memory::module_base_address(game_pid, "libengine2.so");
  printf("libengine2.so  %p\n", (void *)engine_address);

  const uintptr_t resource_offset =
      Memory::interface_offset(game_pid, engine_address, "GameResourceServiceClientV0");
  printf("resource       %p\n", (void *)resource_offset);
  if (!resource_offset) return 1;

  uintptr_t entity_list = 0;
  const bool entity_ok =
      Memory::read(game_pid, resource_offset + 0x50, &entity_list, sizeof(uintptr_t));
  printf("entity list    %p (read %s)\n", (void *)entity_list, entity_ok ? "ok" : "FAILED");

  const uintptr_t local_player = Memory::scan_pattern(
      game_pid, {0x48, 0x83, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x95, 0xC0, 0xC3},
      {true, true, true, false, false, false, false, true, true, true, true, true}, 12,
      client_address);
  printf("local player   %p\n", (void *)local_player);
  if (!local_player) return 1;

  const uintptr_t controller_address =
      Memory::relative_address(game_pid, local_player, 0x3, 0x8);
  printf("controller     %p\n", (void *)controller_address);

  uintptr_t controller = 0;
  Memory::read(game_pid, controller_address, &controller, sizeof(uintptr_t));
  printf("controller ptr %p\n", (void *)controller);

  uintptr_t pawn = 0;
  Memory::read(game_pid, controller + 0x7B4, &pawn, sizeof(uintptr_t));
  printf("pawn           %p\n", (void *)pawn);

  uintptr_t pawn2 = 0;
  Memory::read(game_pid, (entity_list + 0x10) + 8 * ((pawn & 0x7FFF) >> 9), &pawn2,
               sizeof(uintptr_t));
  uintptr_t player = 0;
  Memory::read(game_pid, pawn2 + 120 * (pawn & 0x1FF), &player, sizeof(uintptr_t));
  printf("player         %p\n", (void *)player);

  int health = 0;
  Memory::read(game_pid, player + 0x4BC, &health, sizeof(int));
  printf("health         %d\n", health);

  unsigned char team = 0;
  Memory::read(game_pid, player + 0x55B, &team, sizeof(unsigned char));
  printf("team           %d\n", team);

  uintptr_t weapon = 0;
  Memory::read(game_pid, player + 0x1348, &weapon, sizeof(uintptr_t));
  printf("weapon         %p\n", (void *)weapon);

  const uintptr_t view_matrix = Memory::scan_pattern(
      game_pid,
      {0x48, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00,
       0x48, 0x8D, 0x0D},
      {true, true, true, false, false, false, false, true, true, true, false, false, false,
       false, true, true, true},
      17, client_address);
  printf("view matrix    %p\n", (void *)view_matrix);

  return 0;
}
