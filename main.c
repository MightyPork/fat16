#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "fat16.h"



// ------------- test ----------------

BLOCKDEV test;
FILE* testf;

void test_seek(const uint32_t pos)
{
	fseek(testf, pos, SEEK_SET);
}

void test_rseek(const int16_t pos)
{
	fseek(testf, pos, SEEK_CUR);
}

void test_load(void* dest, const uint16_t len)
{
	fread(dest, len, 1, testf);
//	for (int a = 0; a < len; a++) {
//		fread(dest+a, 1, 1, testf);
//	}
}

void test_store(const void* source, const uint16_t len)
{
	fwrite(source, len, 1, testf);

//	for (int a = 0; a < len; a++) {
//		fwrite(source+a, 1, 1, testf);
//	}
}

void test_write(const uint8_t b)
{
	fwrite(&b, 1, 1, testf);
}

uint8_t test_read()
{
	uint8_t a;
	fread(&a, 1, 1, testf);
	return a;
}

void test_open()
{
	test.read = &test_read;
	test.write = &test_write;

	test.load = &test_load;
	test.store = &test_store;

	test.seek = &test_seek;
	test.rseek = &test_rseek;

	testf = fopen("imgs/hamlet.img", "rb+");
}

void test_close()
{
	fflush(testf);
	fclose(testf);
}


// --- testing ---

int main()
{
	test_open();

	// Initialize the FS
	FAT16 fat;
	fat16_init(&test, &fat);

	FAT16_FILE file;
	fat16_root(&fat, &file);

	char str[12];

	printf("Disk label: %s\n", fat16_disk_label(&fat, str));

	do
	{
		if (!fat16_is_file_valid(&file)) continue;

		printf("File name: %s, %c, %d B, @ 0x%x\n",
			   fat16_dispname(&file, str),
			   file.type, file.size, file.clu_start);

	}
	while (fat16_next(&file));

	test_close();

	return 0;
}
