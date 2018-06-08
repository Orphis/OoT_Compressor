#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vector>

#include "findtable.h"
#include "rom.h"
#include "util.h"
#include "yaz0.h"

#define UINTSIZE 0x01000000
#define COMPSIZE 0x02000000
#define DCMPSIZE 0x04000000

void fix_crc(char *);

/* Structs */
typedef struct
{
	uint32_t startV;   /* Start Virtual Address  */
	uint32_t endV;     /* End Virtual Address    */
	uint32_t startP;   /* Start Physical Address */
	uint32_t endP;     /* End Phycical Address   */
}
table_t;

/* Functions */
table_t getTabEnt(uint32_t);
void setTabEnt(uint32_t, table_t);

/* Globals */
std::vector<uint8_t> inROM;
std::vector<uint8_t> outROM;
uint32_t* inTable;
uint32_t* outTable;

int main(int argc, char** argv)
{
	FILE* outFile;
	int32_t tabStart, tabSize, tabCount;
	int32_t size, i;
	table_t tab, tempTab;
	char* name;

	inROM.resize(DCMPSIZE);
	outROM.resize(DCMPSIZE);

	/* Load the ROM into inROM and outROM */
	inROM = loadROM(argv[1]);
	outROM = inROM;

	/* Find table offsets */
	tabStart = findTable(inROM);
	inTable = (uint32_t*)(inROM.data() + tabStart);
	outTable = (uint32_t*)(outROM.data() + tabStart);
	tab = getTabEnt(2);
	printf("tab2: %08x %08x %08x %08x\n", tab.startV, tab.endV, tab.startP, tab.endP);
	tabSize = tab.endV - tab.startV;
	tabCount = tabSize / 16;

	/* Set everything past the table in outROM to 0 */
	memset(outROM.data() + tab.endV, 0, DCMPSIZE - tab.endV);

	for(i = 3; i < tabCount; i++)
	{
		tempTab = getTabEnt(i);
		size = tempTab.endV - tempTab.startV;

		/* Copy if decoded, decode if encoded */
		if(tempTab.endP == 0x00000000)
			memcpy(outROM.data() + tempTab.startV, inROM.data() + tempTab.startP, size);
		else
			yaz0_decode(inROM.data() + tempTab.startP, outROM.data() + tempTab.startV, size);

		/* Clean up outROM's table */
		tempTab.startP = tempTab.startV;
		tempTab.endP = 0x00000000;
		setTabEnt(i, tempTab);
	}

	/* Write the new ROM */
	std::string filename(argv[1]);
	auto ext = filename.find_last_of('.');
	std::string outname = filename.substr(0, ext) + "-decomp.z64";

	outFile = fopen(outname.c_str(), "wb");
	fwrite(outROM.data(), 1, DCMPSIZE, outFile);
	fclose(outFile);

	/* I have no idea what's going on with this. I think it's just Nintendo magic */
	fix_crc(name);
	free(name);

	return 0;
}

table_t getTabEnt(uint32_t i)
{
	table_t tab;

	/* First 32 bytes are VROM start address, next 32 are VROM end address */
	/* Next 32 bytes are Physical start address, last 32 are Physical end address */
    tab.startV = byteSwap(inTable[i*4]);
    tab.endV   = byteSwap(inTable[(i*4)+1]);
    tab.startP = byteSwap(inTable[(i*4)+2]);
    tab.endP   = byteSwap(inTable[(i*4)+3]);

	return tab;
}

void setTabEnt(uint32_t i, table_t tab)
{
	/* First 32 bytes are VROM start address, next 32 are VROM end address */
	/* Next 32 bytes are Physical start address, last 32 are Physical end address */
	outTable[i*4]     = byteSwap(tab.startV);
	outTable[(i*4)+1] = byteSwap(tab.endV);
	outTable[(i*4)+2] = byteSwap(tab.startP);
	outTable[(i*4)+3] = byteSwap(tab.endP);
}
