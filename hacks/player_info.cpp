#include "player_info.hpp"

#include <unistd.h>
#include <cstdint>
#include <cstring>

#include "../memory.hpp"
#include "offsets.hpp"
#include "schema.hpp"

namespace {

  /* the identity of an entity carries its handle, the low 16 bits are the
     index the entity list knows it by */
  constexpr int kHandleOffset = 0x10;

  /* the identities are 0x80 bytes apart in the current build */
  constexpr uintptr_t kEntityStride = 0x80;

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

  int entity_index(pid_t game_pid, uintptr_t entity) {
    uint32_t handle = 0;
    Memory::read(game_pid, entity + kHandleOffset, &handle, sizeof(handle));
    return (int)(handle & 0xFFFF);
  }

  std::string read_name(pid_t game_pid, uintptr_t address) {
    std::string name;
    for (int i = 0; i < 64; ++i) {
      char c = 0;
      Memory::read(game_pid, address + i, &c, sizeof(c));
      if (!c) break;
      if (c < 32 || c > 126) return "";
      name += c;
    }
    return name;
  }
}

void players(pid_t game_pid) {
  const uintptr_t array = entity_array(game_pid);
  if (!array) return;

  /* the local player is the one the game predicts ahead of everybody else */
  static bool local_ready = false;
  if (!local_ready) {
    float best_time = -1.0f;
    for (unsigned long i = 1; i <= 64; ++i) {
      const uintptr_t entity = entity_at(game_pid, array, i);
      if (!entity) continue;

      int team = 0;
      Memory::read(game_pid, entity + Offsets::kTeam, &team, sizeof(team));
      if (team < 1 || team > 3) continue;

      float simulation_time = 0.0f;
      Memory::read(game_pid, entity + Offsets::kSimulationTime, &simulation_time,
                   sizeof(simulation_time));
      if (simulation_time > best_time) {
        best_time = simulation_time;
        PlayerInfo::i_local_player = (int)i - 1;
        PlayerInfo::local_pawn_address = entity;
        local_ready = true;
      }
    }
  }

  for (unsigned long i = 1; i <= 64; ++i) {
    const uintptr_t player = entity_at(game_pid, array, i);
    if (!player) {
      PlayerInfo::l_players[i-1] = PlayerInfo::Player();
      continue;
    }

    int health = 0;
    Memory::read(game_pid, player + Offsets::kHealth, &health, sizeof(health));

    int team = 0;
    Memory::read(game_pid, player + Offsets::kTeam, &team, sizeof(team));
    if (team < 1 || team > 3) {
      PlayerInfo::l_players[i-1] = PlayerInfo::Player();
      continue;
    }

    uintptr_t game_scene = 0;
    Memory::read(game_pid, player + Offsets::kGameScene, &game_scene, sizeof(game_scene));
    if (!game_scene) {
      PlayerInfo::l_players[i-1] = PlayerInfo::Player();
      continue;
    }

    float location[3];
    Memory::read(game_pid, game_scene + Offsets::kSceneOrigin, &location, sizeof(location));

    int fov_desired = 0;
    Memory::read(game_pid, player + Offsets::kViewAngles, &fov_desired, sizeof(fov_desired));

    float view_angles[2];
    Memory::read(game_pid, player + Offsets::kViewAngles, &view_angles, sizeof(view_angles));

    /* the aim punch lives in its own service */
    uintptr_t aim_punch_service = 0;
    Memory::read(game_pid, player + Offsets::kAimPunch, &aim_punch_service,
                 sizeof(aim_punch_service));

    uintptr_t aim_punch_data_address = 0;
    Memory::read(game_pid, aim_punch_service + 0x8, &aim_punch_data_address,
                 sizeof(aim_punch_data_address));

    float aim_punch[2] = {0.0f, 0.0f};
    if (aim_punch_data_address) {
      Memory::read(game_pid, aim_punch_data_address, aim_punch, sizeof(aim_punch));
    }

    float fov_multiplier = 0.0f;
    Memory::read(game_pid, player + 0x12C4, &fov_multiplier, sizeof(fov_multiplier));

    /* the bones of the skeleton */
    uintptr_t bone_matrix_ptr = 0;
    float bone_matrix[80][3];
    Memory::read(game_pid, game_scene + 0x1F0, &bone_matrix_ptr, sizeof(bone_matrix_ptr));
    for (int b = 0; bone_matrix_ptr != NULL && b < 80; ++b) {
      Memory::read(game_pid, bone_matrix_ptr + b * 32, &bone_matrix[b][0], sizeof(float[3]));
    }

    /* who is holding what */
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
    if (weapon_entity) {
      uintptr_t name_address = 0;
      Memory::read(game_pid, weapon_entity + 0x20, &name_address, sizeof(name_address));
      if (name_address > 0x1000) weapon_name = read_name(game_pid, name_address);
    }

    /* the name of the player, the controller holds it */
    std::string name;
    uint32_t controller_handle = 0;
    Memory::read(game_pid, player + Offsets::kControllerHandle, &controller_handle,
                 sizeof(controller_handle));
    if (controller_handle & 0x7FFF) {
      const uintptr_t controller = entity_at(game_pid, array, controller_handle & 0x7FFF);
      if (controller) {
        uintptr_t name_address = 0;
        Memory::read(game_pid, controller + Offsets::kControllerName, &name_address,
                     sizeof(name_address));
        if (name_address > 0x1000) name = read_name(game_pid, name_address);
      }
    }

    unsigned char flags2 = 0;
    Memory::read(game_pid, player + 0x564, &flags2, sizeof(flags2));
    bool crouched = flags2 & (1 << 1);
    bool on_ground = flags2 & (1 << 0);

    float height = 0.0f;
    Memory::read(game_pid, player + 0xC40, &height, sizeof(height));

    uint64_t spotted_mask = 0;
    Memory::read(game_pid, player + Offsets::kSpottedState, &spotted_mask,
                 sizeof(spotted_mask));
    bool spotted = (spotted_mask & (1ULL << PlayerInfo::i_local_player)) != 0;

    if (player == PlayerInfo::local_pawn_address) {
      PlayerInfo::local_weapon_address = weapon_entity;
    }

    PlayerInfo::l_players[i-1] = PlayerInfo::Player(i-1, health, team, fov_desired,
                                                    crouched, on_ground, spotted,
                                                    fov_multiplier, fov_desired,
                                                    height,
                                                    view_angles, aim_punch,
                                                    location, bone_matrix,
                                                    PlayerInfo::l_players[i-1].fov_distance,
                                                    name, weapon_name);
  }
}
