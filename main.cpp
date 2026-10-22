#include <unistd.h>
#include <stdio.h>
#include <thread>

#include "hacks/player_info.hpp"
#include "hacks/create_move.hpp"
#include "hacks/draw.hpp"
#include "hacks/schema.hpp"

#include "gui/gui.hpp"

#include "process.hpp"
#include "memory.hpp"
#include "client.hpp"
#include "engine.hpp"
#include "xutil.hpp"

int main(int argc, char *argv[]) {
  if (getenv("FLER_GUI_ONLY")) { //menu preview without the game attached
    gui(argc, argv);
    return 0;
  }

  if (getuid()) {
    /* root is only needed when the game runs as a different user */
    printf("Running as a normal user, this only works when the game runs as the same one\n");
  }
  pthread_setname_np(pthread_self(), "main_thread");

  pid_t game_pid = Process::name_to_pid("cs2");
  if (!game_pid) {
    printf("Game not open\n");
    return 1;
  }
  
  printf("Game pid: %d\n", game_pid);  
  
  /* X11 initiation — the display is only used to check which window has the
     focus, the overlay itself is drawn by Qt */
  Display* display = XOpenDisplay(NULL);
  if (!display) return 1;
  Display* move_display = XOpenDisplay(NULL);
  if (!move_display) return 1;
  int screen = DefaultScreen(display);

  Engine::window_width = XDisplayWidth(display, screen);
  Engine::window_height = XDisplayHeight(display, screen);
  printf("screen: %dx%d\n", Engine::window_width, Engine::window_height);

  const uintptr_t client_address = Memory::module_base_address(game_pid, "libclient.so");
  printf("client: %p\n", client_address);
  
  const uintptr_t engine_address = Memory::module_base_address(game_pid, "libengine2.so");
  printf("engine: %p\n", engine_address);
  
  const uintptr_t resource_offset = Memory::interface_offset(game_pid, engine_address, "GameResourceServiceClientV0");
  printf("resource: %p\n", resource_offset);

  Memory::read(game_pid, resource_offset + 0x50, &PlayerInfo::entity_list, sizeof(uintptr_t));

  /* The schema of the running game holds the offset of every field, so an
     update of Counter-Strike cannot break the cheat. */
  if (Schema::dump(game_pid)) {
    printf("schema: %d classes\n", Schema::class_count());
  } else {
    printf("schema: %s (falling back to the built in offsets)\n", Schema::status().c_str());
  }

  Client::view_matrix = Memory::scan_pattern(game_pid,
					     {0x48, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x0D},
					     {
					       true,
					       true,
					       true,
					       false,
					       false,
					       false,
					       false,
					       true,
					       true,
					       true,
					       false,
					       false,
					       false,
					       false, 
					       true,
					       true,
					       true,
					     },
					     17, client_address);
  if (!Client::view_matrix) { printf("Couldn't find view matrix\n"); return 1; }
  Client::view_matrix = Memory::relative_address(game_pid, Client::view_matrix + 0x7, 0x3, 0x7);
  
  Client::view_angles = client_address + 0x39905E0; //this would be better as a pattern
  printf("view_angles: %p\n", Client::view_angles);

  Client::force_attack = client_address + 0x39904A0; //this too!
  printf("force_attack: %p\n", Client::force_attack);

  //start all other thread loops
  std::thread player_thread([game_pid]() {
    for (;;) {
      players(game_pid);
      usleep(1000*1000/250);
    }
  });
  pthread_setname_np(player_thread.native_handle(), "player_thread");

  std::thread move_thread(create_move, game_pid, move_display);
  pthread_setname_np(move_thread.native_handle(), "move_thread");

  /* Qt owns the main thread: the event loop must not live in a worker. */
  gui(argc, argv);

  return 0;
}
