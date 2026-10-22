#include "schema.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "../memory.hpp"

namespace Schema {

  namespace {

    /* The layout of the schema system inside the linux build. The classes of
       a scope live in a UtlTsHash, the fields of a class are a plain array. */
    struct SchemaField {
      const char *name;
      void *type;
      int32_t offset;
      int32_t metadata_count;
      uintptr_t metadata;
    };

    struct SchemaClass {
      uintptr_t base;
      const char *name;
      const char *binary_name;
      const char *module_name;
      int32_t class_size;
      int16_t field_size;
      int16_t static_metadata_count;
      uint8_t pad0[2];
      uint8_t alignment;
      uint8_t has_base_class;
      int16_t total_class_size;
      int16_t derived_class_size;
      SchemaField *fields;
      uint8_t pad1[8];
      uintptr_t base_classes;
      uintptr_t static_metadata;
      uintptr_t type_scope;
      uintptr_t type;
      uint8_t pad2[0x10];
    };

    struct HashNode {
      uint64_t key;
      uintptr_t next;
      uintptr_t data;
    };

    struct HashBucket {
      uintptr_t add_lock;
      uintptr_t first;
      uintptr_t first_uncommitted;
    };

    /* the elements that are not in a bucket any more */
    struct HashBlob {
      uintptr_t next;
      uint8_t pad0[8];
      uintptr_t data;
      uint8_t pad1[0x18];
    };

    /* the memory pool of the hash, the free list lives in it */
    struct MemoryPool {
      int32_t block_size;
      int32_t blocks_per_blob;
      uint32_t grow_mode;
      int32_t blocks_allocated;
      int32_t peak_allocated;
      uint16_t alignment;
      uint16_t blob_count;
      uint8_t pad0[2];
      uintptr_t free_blocks;
      uint8_t pad1[0x20];
      uintptr_t blob_head;
      int32_t total_size;
      uint8_t pad2[0xC];
    };

    constexpr uintptr_t kScopeClassHash = 0x560; /* the UtlTsHash of the classes */
    constexpr uintptr_t kHashMemoryPool = 0x60;  /* size of the memory pool */
    constexpr int kHashBuckets = 256;

    std::unordered_map<std::string, std::vector<ClassInfo>> g_classes;
    bool g_ready = false;
    pid_t g_pid = 0;
    std::string g_status = "not dumped";

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

    /* the field of a class, walking the base classes by pointer so the copy
       from the right module is used */
    int find_in_class(pid_t pid, uintptr_t data, const char *field_name, int depth) {
      if (!data || depth > 12) return -1;

      SchemaClass klass{};
      if (!Memory::read(pid, data, &klass, sizeof(klass))) return -1;
      if (!klass.fields || klass.field_size <= 0 || klass.field_size > 4096) return -1;

      std::vector<SchemaField> field_list(klass.field_size);
      if (!Memory::read(pid, (uintptr_t)klass.fields, field_list.data(),
                        field_list.size() * sizeof(SchemaField))) {
        return -1;
      }

      for (const SchemaField &field : field_list) {
        if (!field.name) continue;
        const std::string name = read_cstring(pid, (uintptr_t)field.name);
        if (!is_field_name(name)) continue;
        if (field.offset >= (uint32_t)klass.class_size) continue;
        if (name == field_name) return (int)field.offset;
      }

      /* the base classes are an array of {pad, class data}, the pointer to it
         sits right behind the fields pointer */
      uintptr_t base_classes = 0;
      if (!Memory::read(pid, data + 0x38, &base_classes, sizeof(base_classes))) return -1;
      if (!base_classes) return -1;

      for (uint8_t i = 0; i < 8; ++i) {
        uintptr_t base = 0;
        if (!Memory::read(pid, base_classes + i * 16 + 8, &base, sizeof(base))) break;
        if (!base) continue;

        /* a class data starts with a pointer to its name */
        uintptr_t name_pointer = 0;
        if (!Memory::read(pid, base + 0x08, &name_pointer, sizeof(name_pointer))) break;
        if (!name_pointer) continue;
        const std::string base_name = read_cstring(pid, name_pointer, 64);
        if (!is_field_name(base_name)) continue;

        const int found = find_in_class(pid, base, field_name, depth + 1);
        if (found >= 0) return found;
      }

      return -1;
    }
  }

  bool dump(pid_t game_pid) {
    g_classes.clear();
    g_ready = false;
    g_pid = game_pid;
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

    /* the type scopes live in a vector behind the interface object */
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

      std::vector<uintptr_t> bindings;

      for (int bucket = 0; bucket < kHashBuckets; ++bucket) {
        const uintptr_t bucket_address =
            scope_address + kScopeClassHash + kHashMemoryPool + bucket * sizeof(HashBucket);

        uintptr_t node_address = 0;
        Memory::read(game_pid, bucket_address + 0x10, &node_address, sizeof(node_address));

        int guard = 0;
        while (node_address && guard++ < 8192) {
          HashNode node{};
          if (!Memory::read(game_pid, node_address, &node, sizeof(node))) break;

          if (node.data) {
            bindings.push_back(node.data);
            SchemaClass klass{};
            if (Memory::read(game_pid, node.data, &klass, sizeof(klass)) && klass.name &&
                klass.fields && klass.field_size > 0 && klass.field_size < 4096) {
              const std::string class_name = read_cstring(game_pid, (uintptr_t)klass.name);
              if (!class_name.empty()) {
                ClassInfo info;
                info.data = node.data;
                std::unordered_map<std::string, int> &fields = info.fields;

                std::vector<SchemaField> field_list(klass.field_size);
                if (Memory::read(game_pid, (uintptr_t)klass.fields, field_list.data(),
                                field_list.size() * sizeof(SchemaField))) {
                  for (const SchemaField &field : field_list) {
                    if (!field.name) continue;
                    const std::string field_name = read_cstring(game_pid, (uintptr_t)field.name);
                    if (!is_field_name(field_name)) continue;
                    if (field.offset >= (uint32_t)klass.class_size) continue;
                    fields[field_name] = (int)field.offset;
                  }
                }

                g_classes[class_name].push_back(info);
              }
            }
          }

          node_address = node.next;
        }
      }

      /* the classes that were removed from the buckets are still in the free
         list of the memory pool */
      MemoryPool pool{};
      if (Memory::read(game_pid, scope_address + kScopeClassHash, &pool, sizeof(pool)) &&
          pool.free_blocks) {
        uintptr_t blob_address = pool.free_blocks;
        int guard = 0;
        while (blob_address && guard++ < 8192) {
          HashBlob blob{};
          if (!Memory::read(game_pid, blob_address, &blob, sizeof(blob))) break;
          if (blob.data) bindings.push_back(blob.data);
          blob_address = blob.next;
        }
      }

      for (uintptr_t binding : bindings) {
        SchemaClass klass{};
        if (!Memory::read(game_pid, binding, &klass, sizeof(klass))) continue;
        if (!klass.name || !klass.fields || klass.field_size <= 0 || klass.field_size > 4096) continue;

        const std::string class_name = read_cstring(game_pid, (uintptr_t)klass.name);
        if (class_name.empty()) continue;

        ClassInfo info;
        info.data = binding;
        std::unordered_map<std::string, int> &fields = info.fields;

        std::vector<SchemaField> field_list(klass.field_size);
        if (Memory::read(game_pid, (uintptr_t)klass.fields, field_list.data(),
                         field_list.size() * sizeof(SchemaField))) {
          for (const SchemaField &field : field_list) {
            if (!field.name) continue;
            const std::string field_name = read_cstring(game_pid, (uintptr_t)field.name);
            if (!is_field_name(field_name)) continue;
            if (field.offset >= (uint32_t)klass.class_size) continue;
            fields[field_name] = (int)field.offset;
          }
        }

        g_classes[class_name].push_back(info);
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
    auto class_it = g_classes.find(class_name);
    if (class_it != g_classes.end()) {
      for (const ClassInfo &info : class_it->second) {
        const int found = find_in_class(g_pid, info.data, field_name, 0);
        if (found >= 0) return found;
      }
    }

    /* last resort: the field may be unique across the schema */
    for (const auto &entry : g_classes) {
      for (const ClassInfo &info : entry.second) {
        auto field_it = info.fields.find(field_name);
        if (field_it != info.fields.end()) return field_it->second;
      }
    }

    return -1;
  }

  int class_count() { return (int)g_classes.size(); }

  std::string status() { return g_status; }

  const std::unordered_map<std::string, std::vector<ClassInfo>> &classes() {
    return g_classes;
  }
}
