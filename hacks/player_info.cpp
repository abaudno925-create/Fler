#include "player_info.hpp"

#include <unistd.h>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "../memory.hpp"
#include "offsets.hpp"
#include "schema.hpp"

namespace {

  /* the identities of the entities sit 0x80 bytes apart in the current build */
  constexpr uintptr_t kEntityStride = 0x80;

  /* the model is about 72 units tall, the eye sits near the top */
  constexpr float kModelHeight = 72.0f;
  constexpr float kEyeHeight = 62.0f;

  uintptr_t entity_array(pid_t game_pid) {
    uintptr_t array = 0;
    Memory::read(game_pid, PlayerInfo::entity_list + 0x10, &array, sizeof(array));
    return array;
  }

  uintptr_t entity_at(pid_t game_pid, uintptr_t array, uintptr_t index) {
    uintptr_t entity = 0;
    Memory::read(game_pid, array + kEntityStride * index, &entity, sizeof(entity));
    return entity;
  }

  bool is_entity(uintptr_t address) {
    return address > 0x100000000ULL && address < 0x700000000000ULL;
  }
}

void players(pid_t game_pid) {
  const uintptr_t array = entity_array(game_pid);
  if (!array) return;

  /* the local player: the one the mouse moves. the view angles of everybody
     else stay put, so the angles that change belong to the local pawn */
  static float last_angles[512][2] = {{0.0f, 0.0f}};
  static int moves[512] = {0};
  static int local_index = -1;
  static int best_moves = 0;

  for (unsigned long i = 0; i < 512; ++i) {
    const uintptr_t player = entity_at(game_pid, array, i);
    if (!is_entity(player)) {
      PlayerInfo::l_players[i] = PlayerInfo::Player();
      continue;
    }

    int team = 0;
    Memory::read(game_pid, player + Offsets::kTeam, &team, sizeof(team));
    if (team < 1 || team > 3) {
      PlayerInfo::l_players[i] = PlayerInfo::Player();
      continue;
    }

    uintptr_t game_scene = 0;
    Memory::read(game_pid, player + Offsets::kGameScene, &game_scene, sizeof(game_scene));
    if (!is_entity(game_scene)) {
      PlayerInfo::l_players[i] = PlayerInfo::Player();
      continue;
    }

    float location[3];
    Memory::read(game_pid, game_scene + Offsets::kSceneOrigin, location, sizeof(location));

    int health = 0;
    Memory::read(game_pid, player + Offsets::kHealth, &health, sizeof(health));

    float view_angles[2] = {0.0f, 0.0f};
    Memory::read(game_pid, player + Offsets::kViewAngles, view_angles, sizeof(view_angles));

    float simulation_time = 0.0f;
    Memory::read(game_pid, player + Offsets::kSimulationTime, &simulation_time,
                 sizeof(simulation_time));

    /* The bones are not part of the schema, so the skeleton is rebuilt from
       the origin: the model stands on it and is 72 units tall. */
    float bone_matrix[80][3];
    memset(bone_matrix, 0, sizeof(bone_matrix));

    const float base_x = location[0];
    const float base_y = location[1];
    const float base_z = location[2];

    auto set_bone = [&](int index, float x, float y, float z) {
      bone_matrix[index][0] = x;
      bone_matrix[index][1] = y;
      bone_matrix[index][2] = z;
    };

    set_bone(0, base_x, base_y, base_z);                     /* root */
    set_bone(1, base_x, base_y, base_z + 56.0f);             /* neck */
    set_bone(4, base_x - 12.0f, base_y, base_z + 48.0f);     /* shoulder */
    set_bone(5, base_x + 12.0f, base_y, base_z + 48.0f);
    set_bone(6, base_x, base_y, base_z + kEyeHeight);         /* head */
    set_bone(22, base_x - 8.0f, base_y, base_z + 36.0f);      /* hips */
    set_bone(23, base_x + 8.0f, base_y, base_z + 36.0f);
    set_bone(24, base_x - 9.0f, base_y, base_z + 18.0f);      /* knees */
    set_bone(25, base_x + 9.0f, base_y, base_z + 18.0f);
    set_bone(26, base_x - 9.0f, base_y, base_z + 2.0f);       /* feet */
    set_bone(27, base_x + 9.0f, base_y, base_z + 2.0f);
    set_bone(29, base_x, base_y, base_z);   /* the feet, the box starts here */

    /* the weapon of the player */
    uintptr_t weapon_services = 0;
    Memory::read(game_pid, player + Offsets::kWeaponServices, &weapon_services,
                 sizeof(weapon_services));

    uint32_t active_weapon = 0;
    if (weapon_services) {
      Memory::read(game_pid, weapon_services + Offsets::kPawnHandle, &active_weapon,
                   sizeof(active_weapon));
    }

    uintptr_t weapon_entity = 0;
    if (active_weapon & 0x7FFF) {
      weapon_entity = entity_at(game_pid, array, active_weapon & 0x7FFF);
    }

    std::string weapon_name;
    if (is_entity(weapon_entity)) {
      uintptr_t name_address = 0;
      Memory::read(game_pid, weapon_entity + 0x20, &name_address, sizeof(name_address));
      if (name_address > 0x1000) {
        for (int c = 0; c < 64; ++c) {
          char letter = 0;
          Memory::read(game_pid, name_address + c, &letter, sizeof(letter));
          if (!letter) break;
          if (letter < 32 || letter > 126) {
            weapon_name.clear();
            break;
          }
          weapon_name += letter;
        }
      }
    }

    /* The local player is the only one whose view angles the mouse moves,
       the bots keep theirs at zero. */
    const bool angles_sane = fabsf(view_angles[0]) <= 90.0f && fabsf(view_angles[1]) <= 180.0f &&
                            (location[0] != 0.0f || location[1] != 0.0f);
    if (angles_sane) {
      if (fabsf(last_angles[i][0] - view_angles[0]) > 0.01f ||
          fabsf(last_angles[i][1] - view_angles[1]) > 0.01f) {
        ++moves[i];
        if (moves[i] > best_moves) {
          best_moves = moves[i];
          local_index = (int)i;
        }
      }
    }

    last_angles[i][0] = view_angles[0];
    last_angles[i][1] = view_angles[1];

    if (local_index == (int)i) {
      PlayerInfo::i_local_player = (int)i;
      PlayerInfo::local_pawn_address = player;
      PlayerInfo::local_weapon_address = weapon_entity;
      PlayerInfo::local_view_angles[0] = view_angles[0];
      PlayerInfo::local_view_angles[1] = view_angles[1];
      PlayerInfo::local_origin[0] = location[0];
      PlayerInfo::local_origin[1] = location[1];
      PlayerInfo::local_origin[2] = location[2];
    }

    PlayerInfo::l_players[i] = PlayerInfo::Player((int)i, health, team, 90,
                                                  false, false, false,
                                                  1.0f, 90, kModelHeight,
                                                  view_angles, view_angles,
                                                  location, bone_matrix,
                                                  PlayerInfo::l_players[i].fov_distance,
                                                  "", weapon_name);
  }
}
