/* Dumps the schema of the running game to text — the same data the cheat
   uses at runtime.

   g++ -O2 -I. tools/schema_dump.cpp hacks/schema.cpp -o /tmp/schema_dump
   /tmp/schema_dump > /tmp/schema.txt
*/

#include <cstdio>
#include <fstream>
#include <string>

#include "../process.hpp"
#include "../hacks/schema.hpp"

int main(int argc, char **argv) {
  const pid_t pid = Process::name_to_pid("cs2");
  if (!pid) {
    printf("cs2 is not running\n");
    return 1;
  }

  if (!Schema::dump(pid)) {
    printf("dump failed: %s\n", Schema::status().c_str());
    return 1;
  }
  printf("classes: %d\n", Schema::class_count());

  const char *path = argc > 1 ? argv[1] : "/tmp/schema.txt";
  FILE *out = fopen(path, "w");
  if (!out) {
    printf("cannot write %s\n", path);
    return 1;
  }

  const char *wanted[] = {"C_BaseEntity", "C_CSPlayerPawn", "C_CSPlayerController",
                          "C_BasePlayerPawn", "C_EconEntity", "C_EconItemView",
                          "C_AttributeContainer", "CPlayer_WeaponServices"};

  for (const char *class_name : wanted) {
    const int health = Schema::offset(class_name, "m_iHealth");
    printf("%-28s health=%d\n", class_name, health);
  }

  for (const auto &entry : Schema::classes()) {
    fprintf(out, "%s\n", entry.first.c_str());
    for (const auto &field : entry.second) {
      fprintf(out, "    %s = 0x%X\n", field.first.c_str(), field.second);
    }
  }
  fclose(out);
  printf("wrote %s\n", path);
  return 0;
}
