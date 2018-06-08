#include <algorithm>
#include <vector>
#include <stdio.h>
#include <stdint.h>

#include "util.h"

#define UINTSIZE 0x1000000
#define COMPSIZE 0x2000000


uint32_t findTable(const std::vector<uint8_t>& inROM)
{
	std::vector<uint8_t> marker{0x7A, 0x65, 0x6C, 0x64, 0x61, 0x40, 0x73, 0x72, 0x64};
	auto it = std::search(inROM.begin(), inROM.end(), marker.begin(), marker.end());
	
	if (it == inROM.end()) {
		fprintf(stderr, "Error: Couldn't find file table\n");
		exit(1);
	}

	it += 32;

	std::vector<uint8_t> marker2{0x00, 0x00, 0x10, 0x60};
	std::vector<uint8_t>::const_iterator final_position;
	do {
		final_position = std::search(it, inROM.end(), marker2.begin(), marker2.end());
		if(final_position != inROM.end()) {
			break;
		}
		it += 16;
	} while(final_position < inROM.end());

	return static_cast<uint32_t>(final_position - inROM.begin() - 4);
}

