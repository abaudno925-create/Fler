#ifndef FLER_SKIN_CHANGER_HPP
#define FLER_SKIN_CHANGER_HPP

#include <sys/types.h>

#include <string>

/* Applies a paint kit to the weapon the local player holds by writing into
   the game process from the outside. The schema offsets shift with every
   Counter-Strike update, so they are calibrated against an offset that is
   known to work (the health field) and every write is guarded by a sanity
   check — a stale schema disables the feature instead of corrupting the
   game. */
namespace SkinChanger {

  void init(pid_t game_pid);

  /* Called from the move loop, reapplies the skin when the game resets it. */
  void update(pid_t game_pid);

  bool ready();
  std::string status();
}

#endif
