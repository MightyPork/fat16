#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "fat16.h"



// ------------- test ----------------

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

void test_write16(const uint16_t b)
{
	fwrite(&b, 2, 1, testf);
}

uint8_t test_read()
{
	uint8_t a;
	fread(&a, 1, 1, testf);
	return a;
}

uint16_t test_read16()
{
	uint16_t a;
	fread(&a, 2, 1, testf);
	return a;
}

void test_open()
{
	test.read = &test_read;
	test.write = &test_write;

	test.read16 = &test_read16;
	test.write16 = &test_write16;

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

	//FAT16_FILE file2;
	fat16_open_root(&fat, &file);
	//fat16_newfile(&file, &file2, "nuclear.war");

	printf("Exists? %d\n", fat16_find_file(&file, "HAMLET.TXT"));
	printf("Size: %d\n", file.size);

	test_close();

	return 0;
}
