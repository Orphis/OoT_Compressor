#pragma once

#include <cstdint>
#include <string>
#include <vector>

class N64ROM {
 public:
  N64ROM(std::string file_name);
 
 private:
  void load();

  std::string name;
  std::vector<uint8_t> data;

  static const char* oot_us_v1_0_index;
};

std::vector<uint8_t> loadROM(const char* name);
