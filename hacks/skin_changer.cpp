#include "skin_changer.hpp"

#include <cstdint>
#include <cstring>
#include <ctime>

#include "../gui/config.hpp"
#include "../memory.hpp"
#include "player_info.hpp"

namespace SkinChanger {

  namespace {

    /* Offsets as of the schema dump that came with the skin changer sources
       (July 2026). They move with every game update, so they get shifted by
       the delta between that dump and the health field, which is known to
       still work in this build. */
    struct Schema {
      static constexpr int kHealth = 0x34C;         /* C_BaseEntity.m_iHealth */
      static constexpr int kAttributesInitialized = 4512; /* C_EconEntity.m_bAttributesInitialized */
      static constexpr int kAttributeManager = 4520;      /* C_EconEntity.m_AttributeManager */
      static constexpr int kFallbackPaintKit = 5760;      /* C_EconEntity.m_nFallbackPaintKit */
      static constexpr int kFallbackSeed = 5764;          /* C_EconEntity.m_nFallbackSeed */
      static constexpr int kFallbackWear = 5768;          /* C_EconEntity.m_flFallbackWear */
      static constexpr int kItem = 80;                    /* C_AttributeContainer.m_Item */
      static constexpr int kEntityQuality = 444;          /* C_EconItemView.m_iEntityQuality */
      static constexpr int kItemIdHigh = 464;             /* C_EconItemView.m_iItemIDHigh */
      static constexpr int kItemIdLow = 468;              /* C_EconItemView.m_iItemIDLow */
      static constexpr int kAttributeList = 520;          /* C_EconItemView.m_AttributeList */
      static constexpr int kAttributes = 8;               /* CAttributeList.m_Attributes */
    };

    /* the health field as this build has it — the calibration anchor */
    constexpr int kKnownHealthOffset = 0x4BC;

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

    int g_delta = 0;
    bool g_ready = false;
    std::string g_status = "not initialised";
    uint64_t g_last_apply = 0;

    int shifted(int offset) { return offset + g_delta; }

    bool validate(pid_t game_pid, uintptr_t weapon) {
      if (!weapon) {
        g_status = "no weapon in hand";
        return false;
      }

      const uintptr_t item = weapon + shifted(Schema::kAttributeManager) + Schema::kItem;

      int32_t quality = 0;
      Memory::read(game_pid, item + Schema::kEntityQuality, &quality, sizeof(quality));
      if (quality < 0 || quality > 12) {
        g_status = "schema offsets are stale — the item view does not look sane";
        return false;
      }

      AttributeVector vector{};
      Memory::read(game_pid, item + Schema::kAttributeList + Schema::kAttributes, &vector,
                   sizeof(vector));
      if (vector.size > 64) {
        g_status = "schema offsets are stale — the attribute list does not look sane";
        return false;
      }
      if (vector.size > 0 && vector.pointer < 0x1000) {
        g_status = "schema offsets are stale — the attribute list points nowhere";
        return false;
      }

      return true;
    }
  }

  void init(pid_t game_pid) {
    g_delta = kKnownHealthOffset - Schema::kHealth;
    g_ready = false;
    g_status = "waiting for the game";

    (void)game_pid;
  }

  bool ready() { return g_ready; }

  std::string status() { return g_status; }

  void update(pid_t game_pid) {
    if (!config.skin.enabled || !config.skin.paint) return;

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

    Memory::write(game_pid, weapon + shifted(Schema::kFallbackPaintKit), &paint, sizeof(paint));
    Memory::write(game_pid, weapon + shifted(Schema::kFallbackSeed), &seed, sizeof(seed));
    Memory::write(game_pid, weapon + shifted(Schema::kFallbackWear), &wear, sizeof(wear));

    const uintptr_t item = weapon + shifted(Schema::kAttributeManager) + Schema::kItem;

    /* the attribute list drives the actual material, rewrite it when the
       item already carries one */
    AttributeVector vector{};
    Memory::read(game_pid, item + Schema::kAttributeList + Schema::kAttributes, &vector,
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
    Memory::write(game_pid, weapon + shifted(Schema::kAttributesInitialized), &not_initialized,
                  sizeof(not_initialized));

    uint32_t unset = 0xFFFFFFFF;
    Memory::write(game_pid, item + Schema::kItemIdHigh, &unset, sizeof(unset));
    Memory::write(game_pid, item + Schema::kItemIdLow, &unset, sizeof(unset));

    int32_t unusual = 3;
    Memory::write(game_pid, item + Schema::kEntityQuality, &unusual, sizeof(unusual));
  }
}
