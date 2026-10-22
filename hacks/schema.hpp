#ifndef FLER_SCHEMA_HPP
#define FLER_SCHEMA_HPP

#include <sys/types.h>

#include <cstdint>

#include <string>
#include <unordered_map>
#include <vector>

/* The engine keeps the whole schema in memory: every class with the offset
   of every field. Reading it costs one pass over the schema system and gives
   the cheat offsets that match the running build instead of ones that were
   hardcoded months ago. */
namespace Schema {

  /* One class of the schema. The same name can exist in more than one
     module, so every module keeps its own copy. */
  struct ClassInfo {
    uintptr_t data = 0;
    std::unordered_map<std::string, int> fields;
  };

  /* Walks the schema system of the running game. */
  bool dump(pid_t game_pid);

  bool ready();

  /* Offset of a field inside a class, -1 when it is not in the schema. */
  int offset(const char *class_name, const char *field_name);

  int class_count();
  std::string status();

  /* every class with its fields, for the tools */
  const std::unordered_map<std::string, std::vector<ClassInfo>> &classes();
}

#endif
