#include "rom.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <fstream>
#include <vector>

#include "util.h"

#define UINTSIZE 0x01000000
#define COMPSIZE 0x02000000
#define DCMPSIZE 0x04000000

std::vector<uint8_t> loadROM(const std::string& name);
void fix_crc(std::vector<uint8_t>& data);

N64ROM::N64ROM(std::string file_name) : name(file_name) { load(); }

void N64ROM::load() {
  data = loadROM(name.c_str());
  outdata = data;
  outdata.resize(DCMPSIZE);

  readTable();
}

std::vector<uint8_t> loadROM(const std::string& name) {
  std::vector<uint8_t> result;

  std::ifstream romFile(name, std::ifstream::binary);
  if (!romFile) {
    perror(name.c_str());
    exit(1);
  }

  romFile.seekg(0, std::ios::end);
  size_t size = romFile.tellg();
  romFile.seekg(0, std::ios::beg);

  result.resize(size);
  romFile.read(reinterpret_cast<char*>(result.data()), size);

  uint16_t* tempROM = reinterpret_cast<uint16_t*>(result.data());
  if (result[0] == 0x37) {
    for (size_t i = 0; i < result.size() / 2; i++) {
      tempROM[i] = byteSwap(tempROM[i]);
    }
  }

  return result;
}

size_t N64ROM::findTable() {
  std::vector<uint8_t> marker{0x7A, 0x65, 0x6C, 0x64, 0x61,
                              0x40, 0x73, 0x72, 0x64};
  auto it = std::search(data.begin(), data.end(), marker.begin(), marker.end());

  if (it == data.end()) {
    fprintf(stderr, "Error: Couldn't find file table\n");
    exit(1);
  }

  it += 32;

  std::vector<uint8_t> marker2{0x00, 0x00, 0x10, 0x60};
  std::vector<uint8_t>::const_iterator final_position;
  do {
    final_position =
        std::search(it, data.end(), marker2.begin(), marker2.end());
    if (final_position != data.end()) {
      break;
    }
    it += 16;
  } while (final_position < data.end());

  return final_position - data.begin() - 4;
}

void N64ROM::readTable() {
  table_position = findTable();
  N64ROM::table_entry toc(data, table_position + 2 * sizeof(table_entry));
  auto toc_entries = (toc.endV - toc.startV) / sizeof(N64ROM::table_entry);
  intable.reserve(toc_entries);
  for (int i = 0; i < toc_entries; i++) {
    intable.emplace_back(data,
                         table_position + sizeof(N64ROM::table_entry) * i);
  }
  outtable = intable;
}

void N64ROM::writeTable() {
  for (size_t i = 0; i < outtable.size(); ++i) {
    const table_entry& entry = outtable[i];
    entry.write(outdata, table_position + sizeof(N64ROM::table_entry) * i);
  }
}

void N64ROM::save(const std::string& file_name) {
  writeTable();
  fix_crc();

  std::ofstream os(file_name, std::ofstream::out | std::ofstream::binary |
                                  std::ofstream::trunc);
  os.write(reinterpret_cast<const char*>(outdata.data()), outdata.size());
}

void N64ROM::fix_crc() { ::fix_crc(outdata); }
