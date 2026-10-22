/* Finds the entity list of the running game by looking for a block of
   CEntityIdentity structures. */

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "memory.hpp"
#include "process.hpp"

static const pid_t pid = Process::name_to_pid("cs2");

static bool heap_pointer(uintptr_t value) {
  /* the heap of the game sits below the mmap area, the modules start at 0x7f */
  return value > 0x100000000ULL && value < 0x700000000000ULL;
}

/* an identity looks like: a descriptor pointer and a small name index */
static bool looks_like_identity(uintptr_t address) {
  uintptr_t descriptor = 0;
  int32_t name_index = -1;
  if (!Memory::read(pid, address + 0x08, &descriptor, 8)) return false;
  if (!Memory::read(pid, address + 0x14, &name_index, 4)) return false;
  if (!heap_pointer(descriptor)) return false;
  return name_index >= 0 && name_index < 1000000;
}

static bool identity_block(uintptr_t base, int count) {
  int found = 0;
  for (int i = 0; i < count; ++i) {
    uintptr_t entity = 0;
    if (!Memory::read(pid, base + 120 * i, &entity, 8)) return false;
    if (!entity) continue;
    if (!heap_pointer(entity)) return false;
    if (!looks_like_identity(entity)) return false;
    ++found;
  }
  return found >= 3;
}

int main(int argc, char **argv) {
  const char *module = argc > 1 ? argv[1] : "libclient.so";

  std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");
  std::string line;

  while (std::getline(maps, line)) {
    if (line.find(module) == std::string::npos) continue;
    if (line.find('w') == std::string::npos) continue;

    uintptr_t start = 0, end = 0;
    if (sscanf(line.c_str(), "%lx-%lx", &start, &end) != 2) continue;
    if (end <= start || end - start > 256 * 1024 * 1024) continue;

    const int fd = open(("/proc/" + std::to_string(pid) + "/mem").c_str(), O_RDONLY);
    if (fd < 0) continue;
    std::vector<unsigned char> buffer(end - start);
    const ssize_t got = pread(fd, buffer.data(), buffer.size(), (off_t)start);
    close(fd);
    if (got <= 0) continue;

    for (size_t offset = 0; offset + 8 <= buffer.size(); offset += 8) {
      uintptr_t candidate = 0;
      memcpy(&candidate, &buffer[offset], 8);
      if (!heap_pointer(candidate)) continue;

      /* the list object keeps the identity array at +0x10 */
      uintptr_t array = 0;
      if (!Memory::read(pid, candidate + 0x10, &array, 8)) continue;
      if (!heap_pointer(array)) continue;
      if (!identity_block(array, 8)) continue;

      printf("hit %p +0x%02lx -> object %p array %p\n", (void *)start, offset, (void *)candidate,
             (void *)array);
      for (int i = 0; i < 8; ++i) {
        uintptr_t entity = 0;
        Memory::read(pid, array + 120 * i, &entity, 8);
        printf("    [%d] %p\n", i, (void *)entity);
      }
      return 0;
    }
  }

  printf("nothing found\n");
  return 1;
}
