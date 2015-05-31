#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "fat16.h"



// ------------- test bed ----------------

BLOCKDEV test;
FILE* testf;

void test_seek(const uint32_t pos)
{
	fseek(testf, pos, SEEK_SET);
}

void test_rseek(const uint16_t pos)
{
	fseek(testf, pos, SEEK_CUR);
}

void test_read(void* dest, const uint16_t len)
{
	for (int a = 0; a < len; a++) {
		fread(dest+a, 1, 1, testf);
	}
}

void test_aread(void* dest, const uint16_t len, const uint32_t addr)
{
	test_seek(addr);
	test_read(dest, len);
}

void test_write(const void* source, const uint16_t len)
{
	for (int a = 0; a < len; a++) {
		fwrite(source+a, 1, 1, testf);
	}
}

void test_awrite(const void* source, const uint16_t len, const uint32_t addr)
{
	test_seek(addr);
	test_write(source, len);
}

void test_open()
{
	test.read = &test_read;
	test.aread = &test_aread;
	test.write = &test_write;
	test.awrite = &test_awrite;
	test.seek = &test_seek;
	test.rseek = &test_rseek;

	testf = fopen("imgs/dump_sd.img", "rb+");
}

void test_close()
{
	fflush(testf);
	fclose(testf);
}


// --- testing ---

int main(int argc, char const *argv[])
{
	uint32_t i32;
	uint16_t i16;
	uint8_t i8;

	test_open();

	// Initialize the FS
	FAT16 fat;
	fat16_init(&test, &fat);

	FAT16_FILE file;
	fat16_open_root(&fat, &file);

	char str[12];

	printf("Disk label: %s\n", fat16_volume_label(&fat, str));

	do {
		if (!fat16_is_file_valid(&file)) continue;

		printf("File name: %s, %c, %d B\n",
			fat16_display_name(&file, str),
			file.type, file.size);

	} while (fat16_next(&file));

	test_close();

	return 0;
}
