#include "skin_changer.hpp"

#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>

#include "../gui/config.hpp"
#include "../memory.hpp"
#include "player_info.hpp"
#include "schema.hpp"

namespace SkinChanger {

  namespace {

    /* Every offset comes from the schema of the running game, so a
       Counter-Strike update cannot break the changer. */
    struct Offsets {
      int attributes_initialized = -1;
      int attribute_manager = -1;
      int fallback_paint_kit = -1;
      int fallback_seed = -1;
      int fallback_wear = -1;
      int item = -1;
      int entity_quality = -1;
      int item_id_high = -1;
      int item_id_low = -1;
      int attribute_list = -1;
      int attributes = -1;
      bool ready = false;
    };

    Offsets g_offsets;
    bool g_loaded = false;
    bool g_ready = false;
    uint64_t g_last_apply = 0;
    std::string g_status = "not initialised";

    /* CEconItemAttribute, 0x48 bytes */
    struct ItemAttribute {
      char pad[0x30];
      uint16_t definition_index;
      char pad2[2];
      float value;
      float initial_value;
      int32_t refundable_currency;
      bool set_bonus;
      char pad3[7];
    };

    struct AttributeVector {
      uint64_t size;
      uintptr_t pointer;
    };

    enum : uint16_t {
      kAttributePaint = 6,
      kAttributePattern = 7,
      kAttributeWear = 8,
    };

    int pick(const char *class_name, const char *field) {
      return Schema::offset(class_name, field);
    }

    void load_offsets() {
      if (g_loaded) return;
      g_loaded = true;

      Offsets &o = g_offsets;

      o.attributes_initialized = pick("C_EconEntity", "m_bAttributesInitialized");
      o.attribute_manager = pick("C_EconEntity", "m_AttributeManager");
      o.fallback_paint_kit = pick("C_EconEntity", "m_nFallbackPaintKit");
      o.fallback_seed = pick("C_EconEntity", "m_nFallbackSeed");
      o.fallback_wear = pick("C_EconEntity", "m_flFallbackWear");
      o.item = pick("C_AttributeContainer", "m_Item");
      o.entity_quality = pick("C_EconItemView", "m_iEntityQuality");
      o.item_id_high = pick("C_EconItemView", "m_iItemIDHigh");
      o.item_id_low = pick("C_EconItemView", "m_iItemIDLow");
      o.attribute_list = pick("C_EconItemView", "m_AttributeList");
      o.attributes = pick("CAttributeList", "m_Attributes");

      o.ready = o.attribute_manager >= 0 && o.item >= 0 && o.fallback_paint_kit >= 0 &&
                o.entity_quality >= 0 && o.attribute_list >= 0 && o.attributes >= 0;
    }

    bool validate(pid_t game_pid, uintptr_t weapon) {
      if (!weapon) {
        g_status = "no weapon in hand";
        return false;
      }

      const uintptr_t item = weapon + g_offsets.attribute_manager + g_offsets.item;

      int32_t quality = 0;
      Memory::read(game_pid, item + g_offsets.entity_quality, &quality, sizeof(quality));
      if (quality < 0 || quality > 12) {
        g_status = "the item view does not look sane — schema offsets are stale";
        return false;
      }

      AttributeVector vector{};
      Memory::read(game_pid, item + g_offsets.attribute_list + g_offsets.attributes, &vector,
                   sizeof(vector));
      if (vector.size > 64) {
        g_status = "the attribute list does not look sane — schema offsets are stale";
        return false;
      }
      if (vector.size > 0 && vector.pointer < 0x1000) {
        g_status = "the attribute list points nowhere — schema offsets are stale";
        return false;
      }

      return true;
    }
  }

  void init(pid_t game_pid) {
    load_offsets();
    g_ready = g_offsets.ready;
    g_status = g_ready ? "waiting for the game" : "the schema has no econ offsets";

    (void)game_pid;
  }

  bool ready() { return g_ready; }

  std::string status() { return g_status; }

  void update(pid_t game_pid) {
    if (!config.skin.enabled || !config.skin.paint) return;

    load_offsets();
    if (!g_offsets.ready) {
      g_status = "the schema has no econ offsets";
      return;
    }

    const uintptr_t weapon = PlayerInfo::local_weapon_address;
    if (!weapon) {
      g_status = "no weapon in hand";
      return;
    }

    if (!g_ready) {
      if (!validate(game_pid, weapon)) return;
      g_ready = true;
      g_status = "active";
    }

    /* the game resets the item on a round start, so keep writing it */
    const uint64_t now = (uint64_t)time(nullptr);
    if (now == g_last_apply) return;
    g_last_apply = now;

    int32_t paint = config.skin.paint;
    int32_t seed = config.skin.seed;
    float wear = config.skin.wear;

    Memory::write(game_pid, weapon + g_offsets.fallback_paint_kit, &paint, sizeof(paint));
    Memory::write(game_pid, weapon + g_offsets.fallback_seed, &seed, sizeof(seed));
    Memory::write(game_pid, weapon + g_offsets.fallback_wear, &wear, sizeof(wear));

    const uintptr_t item = weapon + g_offsets.attribute_manager + g_offsets.item;

    /* the attribute list drives the actual material, rewrite it when the
       item already carries one */
    AttributeVector vector{};
    Memory::read(game_pid, item + g_offsets.attribute_list + g_offsets.attributes, &vector,
                 sizeof(vector));

    if (vector.size >= 3 && vector.pointer > 0x1000) {
      ItemAttribute attributes[3];
      memset(attributes, 0, sizeof(attributes));

      attributes[0].definition_index = kAttributePaint;
      attributes[0].value = (float)paint;
      attributes[0].initial_value = (float)paint;

      attributes[1].definition_index = kAttributePattern;
      attributes[1].value = (float)seed;
      attributes[1].initial_value = (float)seed;

      attributes[2].definition_index = kAttributeWear;
      attributes[2].value = wear;
      attributes[2].initial_value = wear;

      Memory::write(game_pid, vector.pointer, attributes, sizeof(attributes));
    }

    /* let the client rebuild the item from what we just wrote */
    uint8_t not_initialized = 0;
    Memory::write(game_pid, weapon + g_offsets.attributes_initialized, &not_initialized,
                  sizeof(not_initialized));

    uint32_t unset = 0xFFFFFFFF;
    Memory::write(game_pid, item + g_offsets.item_id_high, &unset, sizeof(unset));
    Memory::write(game_pid, item + g_offsets.item_id_low, &unset, sizeof(unset));

    int32_t unusual = 3;
    Memory::write(game_pid, item + g_offsets.entity_quality, &unusual, sizeof(unusual));
  }
}
