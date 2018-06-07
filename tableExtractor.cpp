#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <vector>

#include "findtable.h"
#include "rom.h"

#define UINTSIZE 0x1000000
#define COMPSIZE 0x2000000

uint32_t byteSwap(uint32_t x) {
	return __builtin_bswap32(x);
}

/* Structs */
typedef struct
{
	uint32_t startV;
	uint32_t endV;
	uint32_t startP;
	uint32_t endP;	
}
table_t;

/* Functions */
table_t getTableEnt(uint32_t);
void errorCheck(int, char**);

/* Globals */
std::vector<uint8_t> inROM;
uint32_t* fileTab;

int main(int argc, char** argv)
{
	FILE* file;
	int32_t tabStart, tabSize, tabCount, i;
	std::vector<uint8_t> refTab;
	table_t tab;

	errorCheck(argc, argv);

	/* Open input, read into inROM */
	inROM = loadROM(argv[1]);

	/* Find file table, write to fileTab */
	tabStart = findTable(inROM);
	fileTab = (uint32_t*)(inROM.data() + tabStart);
	tab = getTableEnt(2);
	tabSize = tab.endV - tab.startV;
	tabCount = tabSize / 16;

	/* Fill refTab with 1 for compressed, 0 otherwise */
	refTab.resize(tabCount);
	for(i = 0; i < tabCount; i++)
	{
		tab = getTableEnt(i);
		refTab[i] = (tab.endP == 0) ? '0' : '1';
	}

	/* Write fileTab to table.bin */	
	file = fopen("table.txt", "w");
	fwrite(refTab.data(), tabCount, 1, file);
	fclose(file);
	
	return 0;
}

table_t getTableEnt(uint32_t i)
{
	table_t tab;

	tab.startV = byteSwap(fileTab[i*4]);
	tab.endV = byteSwap(fileTab[(i*4)+1]);
	tab.startP = byteSwap(fileTab[(i*4)+2]);
	tab.endP = byteSwap(fileTab[(i*4)+3]);

	return(tab);
}

void errorCheck(int argc, char** argv)
{
	int i;
	FILE* file;

	/* Check for arguments */
	if(argc != 2)
	{
		fprintf(stderr, "Usage: TabExt [Input ROM]\n");
		exit(1);
	}

	/* Check that input ROM exists */
	file = fopen(argv[1], "rb");
	if(file == NULL)
	{
		perror(argv[1]);
		fclose(file);
		exit(1);
	}

	/* Check that input ROM is correct size */
	fseek(file, 0, SEEK_END);
	i = ftell(file);
	fclose(file);
	if(i != COMPSIZE)
	{
		fprintf(stderr, "Warning: Invalid input ROM size\n");
		exit(1);
	}
}
