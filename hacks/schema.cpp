#include "schema.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "../memory.hpp"

namespace Schema {

  namespace {

    /* the class table, filled by dump() */
    std::unordered_map<std::string, std::unordered_map<std::string, int>> g_classes;
    std::unordered_map<std::string, std::string> g_parents;
    bool g_ready = false;
    std::string g_status = "not dumped";

    /* layout of the schema structures inside the linux build */
    struct SchemaField {
      const char *name;
      void *type;
      uint32_t offset;
      uint32_t pad;
    };

    struct SchemaBaseClassData {
      uint64_t pad0[2];
      const char *name;
      SchemaBaseClassData *next;
    };

    struct SchemaBaseClassRef {
      uint64_t pad0[3];
      SchemaBaseClassData *cls;
    };

    struct SchemaClass {
      uint64_t pad0;
      const char *name;
      uint64_t pad1;
      const char *name2;
      uint32_t class_size;
      uint16_t field_size;
      uint16_t pad2;
      int8_t alignment;
      char pad3[3];
      uint8_t base_class_count;
      SchemaField *fields;
      uint64_t base_classes;
      SchemaBaseClassRef *derived_class;
    };

    struct SchemaDeclaredClass {
      uint64_t pad0[4];
      SchemaClass *value;
    };

    struct SchemaDeclaredClassEntry {
      uint64_t pad0[2];
      SchemaDeclaredClass *declared_class;
    };

    struct SchemaScope {
      uint64_t pad0;
      char name[0x100];
      SchemaScope *global_scope;
      char pad1[0x360];
      uint16_t class_count;
      SchemaDeclaredClassEntry *classes;
    };

    bool is_field_name(const std::string &name) {
      if (name.size() < 2 || name.size() > 96) return false;
      for (char c : name) {
        if (c < 33 || c > 126) return false;
      }
      const char first = name[0];
      return (first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z') || first == '_';
    }

    std::string read_cstring(pid_t pid, uintptr_t address, size_t limit = 128) {
      std::string out;
      for (size_t i = 0; i < limit; ++i) {
        char c = 0;
        if (!Memory::read(pid, address + i, &c, 1) || c == '\0') break;
        out.push_back(c);
      }
      return out;
    }
  }

  bool dump(pid_t game_pid) {
    g_classes.clear();
    g_parents.clear();
    g_ready = false;
    g_status = "looking for the schema system";

    const uintptr_t module = Memory::module_base_address(game_pid, "libschemasystem.so");
    if (!module) {
      g_status = "libschemasystem.so is not loaded";
      return false;
    }

    const uintptr_t interface_pointer =
        Memory::interface_offset(game_pid, module, "SchemaSystem_001");
    if (!interface_pointer) {
      g_status = "the schema system interface was not found";
      return false;
    }

    /* the type scopes live in a vector right behind the interface object */
    uint32_t scope_count = 0;
    uintptr_t scope_array = 0;
    for (uintptr_t offset = 0x100; offset < 0x400; offset += 8) {
      uint32_t count = 0;
      uintptr_t data = 0;
      if (!Memory::read(game_pid, interface_pointer + offset, &count, sizeof(count))) continue;
      if (!Memory::read(game_pid, interface_pointer + offset + 8, &data, sizeof(data))) continue;
      if (!count || count > 64 || !data) continue;

      uintptr_t first = 0;
      if (!Memory::read(game_pid, data, &first, sizeof(first)) || !first) continue;
      if (read_cstring(game_pid, first + 8, 48).empty()) continue;

      scope_count = count;
      scope_array = data;
      break;
    }

    if (!scope_count || !scope_array) {
      g_status = "the schema scope list looks wrong";
      return false;
    }

    std::vector<uintptr_t> scopes(scope_count);
    if (!Memory::read(game_pid, scope_array, scopes.data(), scopes.size() * sizeof(uintptr_t))) {
      g_status = "could not read the schema scope array";
      return false;
    }

    for (uintptr_t scope_address : scopes) {
      if (!scope_address) continue;

      SchemaScope scope{};
      if (!Memory::read(game_pid, scope_address, &scope, sizeof(scope))) continue;
      if (!scope.classes || !scope.class_count) continue;

      std::vector<SchemaDeclaredClassEntry> entries(scope.class_count + 1);
      if (!Memory::read(game_pid, (uintptr_t)scope.classes, entries.data(),
                        entries.size() * sizeof(SchemaDeclaredClassEntry))) {
        continue;
      }

      for (uint16_t i = 0; i < scope.class_count; ++i) {
        SchemaDeclaredClass declared{};
        if (!entries[i].declared_class) continue;
        if (!Memory::read(game_pid, (uintptr_t)entries[i].declared_class, &declared,
                          sizeof(declared))) {
          continue;
        }
        if (!declared.value) continue;

        SchemaClass klass{};
        if (!Memory::read(game_pid, (uintptr_t)declared.value, &klass, sizeof(klass))) continue;
        if (!klass.name || !klass.fields || !klass.field_size) continue;

        const std::string class_name = read_cstring(game_pid, (uintptr_t)klass.name);
        if (class_name.empty()) continue;

        std::unordered_map<std::string, int> &fields = g_classes[class_name];

        const size_t slots = klass.field_size;
        std::vector<SchemaField> field_list(slots);
        if (!Memory::read(game_pid, (uintptr_t)klass.fields, field_list.data(),
                          field_list.size() * sizeof(SchemaField))) {
          continue;
        }

        for (size_t f = 0; f < field_list.size(); ++f) {
          if (!field_list[f].name) continue;
          const std::string field_name = read_cstring(game_pid, (uintptr_t)field_list[f].name);
          if (!is_field_name(field_name)) continue;
          /* a field can never live past the end of its class */
          if (field_list[f].offset >= klass.class_size) continue;
          fields[field_name] = (int)field_list[f].offset;
        }

        /* remember the parent so lookups can walk the hierarchy */
        if (klass.derived_class) {
          SchemaBaseClassRef parent_ref{};
          if (Memory::read(game_pid, (uintptr_t)klass.derived_class, &parent_ref,
                           sizeof(parent_ref)) &&
              parent_ref.cls) {
            SchemaBaseClassData parent{};
            if (Memory::read(game_pid, (uintptr_t)parent_ref.cls, &parent, sizeof(parent)) &&
                parent.name) {
              g_parents[class_name] = read_cstring(game_pid, (uintptr_t)parent.name);
            }
          }
        }
      }
    }

    if (g_classes.empty()) {
      g_status = "the schema came back empty";
      return false;
    }

    g_ready = true;
    g_status = "dumped";
    return true;
  }

  bool ready() { return g_ready; }

  int offset(const char *class_name, const char *field_name) {
    std::string name = class_name;

    /* walk the inheritance chain, the field usually lives in a base class */
    for (int depth = 0; depth < 16; ++depth) {
      auto class_it = g_classes.find(name);
      if (class_it != g_classes.end()) {
        auto field_it = class_it->second.find(field_name);
        if (field_it != class_it->second.end()) return field_it->second;
      }

      auto parent_it = g_parents.find(name);
      if (parent_it == g_parents.end() || parent_it->second.empty()) break;
      name = parent_it->second;
    }

    /* last resort: the field may be unique across the schema */
    for (const auto &entry : g_classes) {
      auto field_it = entry.second.find(field_name);
      if (field_it != entry.second.end()) return field_it->second;
    }

    return -1;
  }

  int class_count() { return (int)g_classes.size(); }

  std::string status() { return g_status; }

  const std::unordered_map<std::string, std::unordered_map<std::string, int>> &classes() {
    return g_classes;
  }
}
