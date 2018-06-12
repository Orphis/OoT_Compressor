#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ThreadPool.h"
#include "rom.h"
#include "yaz0.h"

#define UINTSIZE 0x1000000
#define COMPSIZE 0x2000000
#define DCMPSIZE 0x4000000

void compression_thread(const uint8_t* data, size_t size,
                        std::vector<uint8_t>& out,
                        std::atomic<int>& thread_count) {
  out = yaz0_encode(data, size);
  thread_count--;
}

int cpu_count() {
  int n = std::thread::hardware_concurrency();
  switch (n) {
    case 0:
      return 2;
    case 1:
      return 3;
    default:
      return n + 2;
  }
}

void compress(const std::string& name, const std::string& outname) {
  N64ROM rom(name);

  // Load the compression index
  const N64ROM::table_entry& compression_index_entry =
      rom.inEntry(rom.entry_count() - 1);
  if (!compression_index_entry.startP) {
    fprintf(stderr,
            "Compression index missing, please use the decompressor from this "
            "repository on the ROM\n");
    exit(1);
  }
  std::vector<uint8_t> compression_index(
      rom.in().data() + compression_index_entry.startP,
      rom.in().data() + compression_index_entry.startP + rom.entry_count());

  std::atomic<int> numThreads = 0;
  std::vector<std::vector<uint8_t>> compressed_data;
  compressed_data.resize(rom.entry_count());

  ThreadPool pool(cpu_count());
  printf("Using %d threads\n", cpu_count());
  for (size_t i = 3; i < rom.entry_count(); i++) {
    if (!compression_index[i]) continue;

    const auto& entry = rom.inEntry(i);
    numThreads++;
    pool.enqueue(compression_thread, rom.in().data() + entry.startP,
                 entry.size(), std::ref(compressed_data[i]),
                 std::ref(numThreads));
  }

  printf("Compressing %d files\n", numThreads.load());
  while (numThreads > 0) {
    printf("~%d threads remaining\n", numThreads.load());
    fflush(stdout);
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }

  /* Setup for copying to outROM */
  rom.out().resize(COMPSIZE);

  size_t write_pointer = rom.inEntry(3).startP;
  memset(rom.out().data() + write_pointer, 0, rom.out().size() - write_pointer);

  /* Copy to outROM loop */
  for (size_t i = 3; i < rom.entry_count(); i++) {
    const auto& entry = rom.inEntry(i);
    auto& outentry = rom.outEntry(i);

    outentry.startP = write_pointer;

    if (compression_index[i]) {
      memcpy(rom.out().data() + write_pointer, compressed_data[i].data(),
             compressed_data[i].size());
      outentry.endP = outentry.startP + compressed_data[i].size();
      write_pointer = outentry.endP;
    } else {
      memcpy(rom.out().data() + write_pointer, rom.in().data() + entry.startP,
             entry.size());
      write_pointer += entry.size();
    }
  }

  rom.save(outname);
}

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    fprintf(stderr, "Usage: %s file [outfile]", argv[0]);
    return 1;
  }

  std::string name = argv[1];
  std::string outname =
      argc == 3 ? argv[2]
                : (name.substr(0, name.find_last_of('.')) + "-comp.z64");

  compress(name, outname);
  return 0;
}
