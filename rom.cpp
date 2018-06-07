#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vector>

#define UINTSIZE 0x01000000
#define COMPSIZE 0x02000000
#define DCMPSIZE 0x04000000
#define bSwap_32(x, y) asm("bswap %%eax" : "=a"(x) : "a"(y))
#define bSwap_16(x, y) asm("xchg %h0, %b0" : "=a"(x) : "a"(y))

std::vector<uint8_t> loadROM(char* name)
{
	uint32_t size, i;
	uint16_t* tempROM;
	FILE* romFile;
	std::vector<uint8_t> inROM;
	
	/* Open file, make sure it exists */
	romFile = fopen(name, "rb");
	if(romFile == NULL)
	{
		perror(name);
		exit(1);
	}
	/* Find size of file */
	fseek(romFile, 0, SEEK_END);
	size = ftell(romFile);
	fseek(romFile, 0, SEEK_SET);

	/* If it's not the right size, exit */
	if(size != COMPSIZE)
	{
		fprintf(stderr, "Error, %s is not the correct size", name);
		exit(1);
	}

	inROM.resize(size);
	/* Read to inROM, close romFile, and copy to outROM */
	fread(inROM.data(), sizeof(char), size, romFile);
	tempROM = (uint16_t*)inROM.data();
	fclose(romFile);

	/* bSwap_32 if needed */
	if (inROM[0] == 0x37)
		for (i = 0; i < UINTSIZE; i++)
			bSwap_16(tempROM[i], tempROM[i]);

	return inROM;
}

