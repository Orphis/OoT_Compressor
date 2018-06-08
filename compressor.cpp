#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "ThreadPool.h"
#include "findtable.h"
#include "util.h"
#include "yaz0.h"

#define UINTSIZE 0x1000000
#define COMPSIZE 0x2000000
#define DCMPSIZE 0x4000000

void fix_crc(char *);

/* Structs */
typedef struct
{
	uint32_t startV;
	uint32_t endV;
	uint32_t startP;
	uint32_t endP;	
}
table_t;

typedef struct 
{
	uint32_t num;
	uint8_t* src;
	int src_size;
	table_t  tab;
}
args_t;

typedef struct
{
	table_t table;
	std::vector<uint8_t> data;
	uint8_t  comp;
}
output_t;

/* Functions */
void getTableEnt(table_t*, uint32_t*, uint32_t);
void thread_func(args_t);
void errorCheck(int, char**);

/* Globals */
std::vector<uint8_t> inROM;
std::vector<uint8_t> outROM;
std::vector<uint8_t> refTab;
std::atomic<uint32_t> numThreads;
std::vector<output_t> out;

int main(int argc, char** argv)
{
	FILE* file;
	uint32_t* fileTab;
	int32_t tabStart, tabSize, tabCount;
	volatile int32_t prev, prev_comp;
	int32_t i, size;
	char* name;
	table_t tab;

	errorCheck(argc, argv);

	/* Open input, read into inROM */
	file = fopen(argv[1], "rb");
	inROM.resize(DCMPSIZE);
	fread(inROM.data(), DCMPSIZE, 1, file);
	fclose(file);

	/* Find the file table and relevant info */
	tabStart = findTable(inROM);
	fileTab = (uint32_t*)(inROM.data() + tabStart);
	getTableEnt(&tab, fileTab, 2);
	tabSize = tab.endV - tab.startV;
	tabCount = tabSize / 16;
	
	/* Read in compression reference */
	file = fopen("table.txt", "r");
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	refTab.resize(size);
	for(i = 0; i < size; i++)
		refTab[i] = (fgetc(file) == '1') ? 1 : 0;
	fclose(file);

	/* Initialise some stuff */
	out.resize(tabCount);
	numThreads = 0;
	ThreadPool pool(8);

	/* Create all the threads */
	for(i = 3; i < tabCount; i++)
	{
		args_t args;

		getTableEnt(&(args.tab), fileTab, i);
		args.num = i;

		numThreads++;

		pool.enqueue(thread_func, args);
	}

	/* Wait for all of the threads to finish */
	while(numThreads > 0)
	{
		printf("~%d threads remaining\n", numThreads.load());
		fflush(stdout);
        std::this_thread::sleep_for(std::chrono::seconds(5));
	}

	/* Setup for copying to outROM */
	outROM.resize(COMPSIZE);
	memcpy(outROM.data(), inROM.data(), tabStart + tabSize);

	prev = tabStart + tabSize;
	prev_comp = refTab[2];
	tabStart += 0x20;
	inROM.clear();

	/* Copy to outROM loop */
	for(i = 3; i < tabCount; i++)
	{
		tab = out[i].table;
		tabStart += 0x10;

		/* Finish table and copy to outROM */
		if(tab.startV != tab.endV)
		{
			tab.startP = prev;
			if(out[i].comp)
				tab.endP = tab.startP + out[i].data.size();
			memcpy(outROM.data() + tab.startP, out[i].data.data(), out[i].data.size());
            tab.startV = byteSwap(tab.startV);
			tab.endV   = byteSwap(tab.endV);
			tab.startP = byteSwap(tab.startP);
			tab.endP   = byteSwap(tab.endP);
			memcpy(outROM.data() + tabStart, &tab, sizeof(table_t));
		}

		/* Setup for next iteration */
		prev += size;
		prev_comp = out[i].comp;
	}
	out.clear();

	/* Make and fill the output ROM */
	std::string filename(argv[1]);
	auto ext = filename.find_last_of('.');
	std::string outname = filename.substr(0, ext) + "-comp.z64";

	file = fopen(outname.c_str(), "wb");
	fwrite(outROM.data(), COMPSIZE, 1, file);
	fclose(file);

	/* Fix the CRC using some kind of magic or something */
	fix_crc(name);
	free(name);
	
	return(0);
}

void getTableEnt(table_t* tab, uint32_t* files, uint32_t i)
{
	tab->startV = byteSwap(files[i*4]);
	tab->endV   = byteSwap(files[(i*4)+1]);
	tab->startP = byteSwap(files[(i*4)+2]);
	tab->endP   = byteSwap(files[(i*4)+3]);
}

void thread_func(args_t a)
{
	table_t t;
	int i;

	t = a.tab;
	i = a.num;

	/* Setup the src*/
	a.src_size = t.endV - t.startV;
	a.src = inROM.data() + t.startV;

	/* If needed, compress and fix size */
	/* Otherwise, just copy src into outROM */
	if(refTab[i])
	{
		out[i].comp = 1;
		out[i].data = yaz0_encode(a.src, a.src_size);
	}
	else
	{
		out[i].comp = 0;
		out[i].data.assign(a.src, a.src + a.src_size);
	}

	out[i].table = t;

	/* Decrement thread counter */
	numThreads--;
}

void errorCheck(int argc, char** argv)
{
	int i;
	FILE* file;

	/* Check for arguments */
	if(argc != 2)
	{
		fprintf(stderr, "Usage: Compress [Input ROM]\n");
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
	if(i != DCMPSIZE)
	{
		fprintf(stderr, "Warning: Invalid input ROM size\n");
		exit(1);
	}

	/* Check that table.bin exists */
	file = fopen("table.txt", "r");
	if(file == NULL)
	{
		perror("table.txt");
		fprintf(stderr, "If you do not have a table.txt file, please use TabExt first\n");
		fclose(file);
		exit(1);
	}
	fclose(file);
}
