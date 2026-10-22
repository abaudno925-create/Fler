#include "player_info.hpp"

#include <unistd.h>
#include <cstdint>

#include "../memory.hpp"
#include "schema.hpp"

namespace {

  /* The offsets come from the schema of the running game, so a Counter-Strike
     update cannot break them. The constants are the fallback for the days
     when the schema cannot be read. */
  struct Offsets {
    int pawn = 0x7B4;        /* controller -> pawn */
    int health = 0x4BC;
    int team = 0x55B;
    int origin = 0xD40;
    int game_scene = 0x4A0;
    int camera_service = 0x1188;
    int view_angles = 0x11F4;
    int weapon = 0x1348;
    int spotted = 0x328C;
    int aim_punch = 0x1550;
    int fov_multiplier = 0x12C4;
    int name = 0x8F8;
    int fov_desired = 0x87C;
    int bone_matrix = 0x1F0; /* game scene + offset */
  };

  Offsets g_offsets;
  bool g_offsets_ready = false;
  uintptr_t g_local_pawn = 0;

  int pick(const char *class_name, const char *field, int fallback) {
    const int value = Schema::offset(class_name, field);
    return value >= 0 ? value : fallback;
  }

  void load_offsets() {
    if (g_offsets_ready) return;
    g_offsets_ready = true;

    Offsets &o = g_offsets;

    const int pawn = pick("CBasePlayerController", "m_hPawn", -1);
    if (pawn >= 0) o.pawn = pawn;

    const int health = pick("C_CSPlayerPawn", "m_iHealth", -1);
    if (health >= 0) o.health = health;

    const int team = pick("C_CSPlayerPawn", "m_iTeamNum", -1);
    if (team >= 0) o.team = team;

    const int origin = pick("C_CSPlayerPawn", "m_vecAbsOrigin", -1);
    if (origin >= 0) o.origin = origin;

    const int scene = pick("C_CSPlayerPawn", "m_pGameSceneNode", -1);
    if (scene >= 0) o.game_scene = scene;

    const int camera = pick("C_CSPlayerPawn", "m_pCameraServices", -1);
    if (camera >= 0) o.camera_service = camera;

    const int angles = pick("C_CSPlayerPawn", "m_angEyeAngles", -1);
    if (angles >= 0) o.view_angles = angles;

    const int weapon = pick("C_CSPlayerPawn", "m_hActiveWeapon", -1);
    if (weapon >= 0) o.weapon = weapon;

    const int spotted = pick("C_CSPlayerPawn", "m_unPlayerSpottedMask", -1);
    if (spotted >= 0) o.spotted = spotted;

    const int punch = pick("C_CSPlayerPawn", "m_pAimPunchServices", -1);
    if (punch >= 0) o.aim_punch = punch;

    const int name = pick("CBasePlayerController", "m_sSanitizedPlayerName", -1);
    if (name < 0) o.name = pick("CCSPlayerController", "m_sSanitizedPlayerName", -1);
    else o.name = name;

    const int desired = pick("CBasePlayerController", "m_iDesiredFOV", -1);
    if (desired >= 0) o.fov_desired = desired;

    const int bones = pick("C_CSPlayerPawn", "m_pBoneArray", -1);
    if (bones >= 0) o.bone_matrix = bones;
  }

  /* the entity list stores the entities by handle index */
  uintptr_t resolve_entity(pid_t game_pid, uintptr_t index) {
    uintptr_t entry = 0;
    if (!Memory::read(game_pid, (PlayerInfo::entity_list + 0x10) + 8 * (index >> 9), &entry,
                      sizeof(entry))) {
      return 0;
    }
    if (!entry) return 0;

    uintptr_t entity = 0;
    if (!Memory::read(game_pid, entry + 120 * (index & 0x1FF), &entity, sizeof(entity))) return 0;
    return entity;
  }

  /* The game predicts the local player ahead of everybody else, so the pawn
     with the newest simulation time belongs to the one sitting at the
     keyboard. That finds the local controller without a version bound
     pattern. */
  uintptr_t find_local_controller(pid_t game_pid, int *index_out) {
    uintptr_t best_controller = 0;
    float best_time = -1.0f;
    int best_index = -1;

    for (unsigned long i = 1; i <= 64; ++i) {
      uintptr_t entry = 0;
      Memory::read(game_pid, PlayerInfo::entity_list + 0x10, &entry, sizeof(entry));
      if (!entry) break;

      uintptr_t controller = 0;
      Memory::read(game_pid, entry + 120 * (i & 0x1FF), &controller, sizeof(controller));
      if (!controller) continue;

      uintptr_t pawn = 0;
      Memory::read(game_pid, controller + g_offsets.pawn, &pawn, sizeof(pawn));
      if (!pawn) continue;

      const uintptr_t resolved = resolve_entity(game_pid, pawn & 0x7FFF);
      if (!resolved) continue;

      float simulation_time = 0.0f;
      Memory::read(game_pid, resolved + 0x528, &simulation_time, sizeof(simulation_time));
      if (simulation_time > best_time) {
        best_time = simulation_time;
        best_controller = controller;
        best_index = (int)(i - 1);
        g_local_pawn = resolved;
      }
    }

    if (index_out) *index_out = best_index;
    return best_controller;
  }
}

void players(pid_t game_pid) {
  load_offsets();

  /* the local player, found without a version bound pattern */
  static bool local_ready = false;
  if (!local_ready) {
    int local_index = -1;
    const uintptr_t local_controller = find_local_controller(game_pid, &local_index);
    if (local_controller) {
      local_ready = true;
      PlayerInfo::i_local_player = local_index;
      PlayerInfo::local_player_controller_address = local_controller;
    }
  }

  for (unsigned long i = 1; i <= 64; ++i) {
    uintptr_t entry;
    Memory::read(game_pid, PlayerInfo::entity_list + 0x10, &entry, sizeof(uintptr_t));
    if (!entry) {
      PlayerInfo::l_players[i-1] = PlayerInfo::Player();
      continue;
    }
    
    uintptr_t controller;
    Memory::read(game_pid, entry + 120 * (i & 0x1FF), &controller, sizeof(uintptr_t));
    if (!controller) {
      PlayerInfo::l_players[i-1] = PlayerInfo::Player();
      continue;
    }

    uintptr_t name_address;
    Memory::read(game_pid, controller + g_offsets.name, &name_address, sizeof(uintptr_t));

    int fov_desired;
    Memory::read(game_pid, controller + g_offsets.name - 0x7C, &fov_desired, sizeof(int));
    
    std::string name;
    for (int h = 0; h < 256; ++h) {
      char c;
      Memory::read(game_pid, name_address + h, &c, sizeof(char));

      if (c == '\0') break;

      name += c;
    }
    
    uintptr_t pawn;
    Memory::read(game_pid, controller + g_offsets.pawn, &pawn, sizeof(uintptr_t));
    if (!pawn) {
      PlayerInfo::l_players[i-1] = PlayerInfo::Player();
      continue;
    }

    //idk anymore
    uintptr_t __pawn2__;
    Memory::read(game_pid, (PlayerInfo::entity_list + 0x10) + 8 * ((pawn & 0x7FFF) >> 9), &__pawn2__, sizeof(uintptr_t));
    if (!__pawn2__) {
      PlayerInfo::l_players[i-1] = PlayerInfo::Player();
      continue;
    }

    uintptr_t player;
    Memory::read(game_pid, __pawn2__ + 120 * (pawn & 0x1FF), &player, sizeof(uintptr_t));
    if (!player) {
      PlayerInfo::l_players[i-1] = PlayerInfo::Player();
      continue;
    }

    uintptr_t game_scene;
    Memory::read(game_pid, player + g_offsets.game_scene, &game_scene, sizeof(uintptr_t));
    if (!game_scene) {
      PlayerInfo::l_players[i-1] = PlayerInfo::Player();
      continue;
    }

    uintptr_t camera_service;
    Memory::read(game_pid, player + g_offsets.camera_service, &camera_service, sizeof(uintptr_t));
    if (!camera_service) {
      PlayerInfo::l_players[i-1] = PlayerInfo::Player();
      continue;
    }    
    
    int fov;
    Memory::read(game_pid, camera_service + 0x210, &fov, sizeof(int));

    if (player == g_local_pawn) {
      PlayerInfo::i_local_player = i-1;
    }

    float fov_multiplier;
    Memory::read(game_pid, player + g_offsets.fov_multiplier, &fov_multiplier, sizeof(float));
    
    uintptr_t bone_matrix_ptr;
    float bone_matrix[80][3];
    Memory::read(game_pid, game_scene + g_offsets.bone_matrix, &bone_matrix_ptr, sizeof(uintptr_t));
    for (int i = 0; bone_matrix_ptr != NULL && i < 80; ++i) {
      Memory::read(game_pid, bone_matrix_ptr + i * 32, &bone_matrix[i][0], sizeof(float[3]));
    }

    uintptr_t aim_punch_length;
    Memory::read(game_pid, player + g_offsets.aim_punch, &aim_punch_length, sizeof(uintptr_t));

    uintptr_t aim_punch_data_address;
    Memory::read(game_pid, player + g_offsets.aim_punch + 0x8, &aim_punch_data_address, sizeof(uintptr_t));

    float aim_punch[2];
    Memory::read(game_pid, aim_punch_data_address + (aim_punch_length -1) * 12, &aim_punch, sizeof(float[2]));

    if (aim_punch_length < 1) {
      aim_punch[0] = 0;
      aim_punch[1] = 0;
    }

    float view_angles[2];
    Memory::read(game_pid, player + g_offsets.view_angles, &view_angles, sizeof(float[2]));
    
    unsigned char team;
    Memory::read(game_pid, player + g_offsets.team, &team, sizeof(unsigned char));

    int health = -1;
    Memory::read(game_pid, player + g_offsets.health, &health, sizeof(int));

    unsigned char flags2;
    Memory::read(game_pid, player + 0x564, &flags2, sizeof(unsigned char));
    bool crouched = flags2 & (1<<1);
    bool on_ground = flags2 & (1<<0);

    float height;
    Memory::read(game_pid, player + 0xC40, &height, sizeof(float));
    
    float location[3];
    Memory::read(game_pid, player + g_offsets.origin, &location, sizeof(float[3]));
    
    unsigned long spotted_mask;
    Memory::read(game_pid, player + g_offsets.spotted, &spotted_mask, sizeof(unsigned long));
    bool spotted = spotted_mask & (1 << PlayerInfo::i_local_player);

    uintptr_t weapon_entity_instance;
    Memory::read(game_pid, player + g_offsets.weapon, &weapon_entity_instance, sizeof(uintptr_t));

    if (player == g_local_pawn) {
      PlayerInfo::local_pawn_address = player;
      PlayerInfo::local_weapon_address = weapon_entity_instance;
    }

    uintptr_t weapon_entity_identity;
    Memory::read(game_pid, weapon_entity_instance + 0x10, &weapon_entity_identity, sizeof(uintptr_t));
    uintptr_t weapon_name_address;
    Memory::read(game_pid, weapon_entity_identity + 0x20, &weapon_name_address, sizeof(uintptr_t));

    std::string weapon_name;
    for (int h = 0; h < 256; ++h) {
      char c;
      Memory::read(game_pid, weapon_name_address + h, &c, sizeof(char));

      if (c == '\0') break;

      weapon_name += c;
    }
        
    
    PlayerInfo::l_players[i-1] = PlayerInfo::Player(i-1, health, team, fov,
						    crouched, on_ground, spotted,
						    fov_multiplier, fov_desired,
						    height,
						    view_angles, aim_punch,
						    location, bone_matrix,
						    PlayerInfo::l_players[i-1].fov_distance,
						    name, weapon_name);
  }
}
