#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "findtable.h"
#include "rom.h"
#include "util.h"
#include "yaz0.h"

#define UINTSIZE 0x01000000
#define COMPSIZE 0x02000000
#define DCMPSIZE 0x04000000

void decompress(const std::string& name, const std::string& outname);

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    fprintf(stderr, "Usage: %s file [outfile]", argv[0]);
    return 1;
  }

  std::string name = argv[1];
  std::string outname =
      argc == 3 ? argv[2]
                : (name.substr(0, name.find_last_of('.')) + "-decomp.z64");

  decompress(name, outname);

  return 0;
}

void decompress(const std::string& name, const std::string& outname) {
  N64ROM rom(name);

  std::vector<uint8_t> compression_index(rom.entry_count());

  const size_t first_file = 3;
  uint32_t last_endv;

  // Set everything from the first file we copy to the end to 0
  memset(rom.out().data() + rom.inEntry(first_file).startP, 0,
         DCMPSIZE - rom.inEntry(first_file).startP);

  for (size_t i = first_file; i < rom.entry_count(); ++i) {
    auto entry = rom.inEntry(i);
    auto& outentry = rom.outEntry(i);
    auto& outpreventry = rom.outEntry(i - 1);

    // Dummy entry, skip it!
    if (!entry.endV) continue;

    if (entry.is_compressed()) {
      yaz0_decode(rom.in().data() + entry.startP,
                  rom.out().data() + entry.startV, entry.size());
      compression_index[i] = 1;
    } else {
      memcpy(rom.out().data() + entry.startV, rom.in().data() + entry.startP,
             entry.size());
    }

    last_endv = entry.endV;
    outentry.startP = entry.startV;
    outentry.endP = 0;
  }

  // Write the list of compressed entries at the back of the decompressed file
  // for later recompression
  auto& compression_index_entry = rom.outEntry(rom.entry_count() - 1);
  compression_index_entry.startP = last_endv;
  memcpy(rom.out().data() + last_endv, compression_index.data(),
         compression_index.size());

  rom.save(outname);
}
