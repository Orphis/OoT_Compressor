#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "util.h"

class N64ROM {
 public:
  struct table_entry {
    table_entry(const std::vector<uint8_t>& data, size_t pos) {
      static_assert(sizeof(table_entry) == 16);
      const uint32_t* data32 = reinterpret_cast<const uint32_t*>(data.data() + pos);
      startV = bigendian(data32[0]);
      endV = bigendian(data32[1]);
      startP = bigendian(data32[2]);
      endP = bigendian(data32[3]);
    }

    uint32_t startV; /* Start Virtual Address  */
    uint32_t endV;   /* End Virtual Address    */
    uint32_t startP; /* Start Physical Address */
    uint32_t endP;   /* End Phycical Address   */

    void write(std::vector<uint8_t>& data, size_t pos) const {
      uint32_t* data32 = reinterpret_cast<uint32_t*>(data.data() + pos);
      data32[0] = bigendian(startV);
      data32[1] = bigendian(endV);
      data32[2] = bigendian(startP);
      data32[3] = bigendian(endP);
    }

    bool is_compressed() const { return endP != 0; }
    uint32_t size() const { return endV - startV; }
  };

  N64ROM(std::string file_name);

  const std::vector<uint8_t>& in() const { return data; }
  std::vector<uint8_t>& out() { return outdata; }

  void fix_crc();
  void save(const std::string& file_name);

  void readTable();
  void writeTable();
  size_t entry_count() const { return intable.size(); }
  const table_entry& inEntry(size_t i) { return intable[i]; }
  table_entry& outEntry(size_t i) { return outtable[i]; }

 private:
  void load();
  size_t findTable();

  std::string name;
  std::vector<uint8_t> data;
  std::vector<uint8_t> outdata;

  size_t table_position;
  std::vector<table_entry> intable;
  std::vector<table_entry> outtable;

  static const char* oot_us_v1_0_index;
};
