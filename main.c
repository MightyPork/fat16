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

	fat16_root(&fat, &file);

//	if (fat16_find(&file, "FOLDER"))
//	{
//		if (fat16_opendir(&file))
//		{
//			printf("Listing FOLDER:\n");

//			do
//			{
//				printf("File name: %s, %c, %d B, @ 0x%x\n",
//					   fat16_dispname(&file, str),
//					   file.type, file.size, file.clu_start);
//			}
//			while (fat16_next(&file));

//			char m[49];

//			if (fat16_find(&file, "banana.exe"))
//			{
//				fat16_fread(&file, m, 49);
//				printf("%.49s\n", m);
//			}

//			fat16_rmfile(&file);

//			if (fat16_parent(&file)) {
//				printf("Current file: %s\n", fat16_dispname(&file, m));
//			}

//			printf("Found? %d\n", fat16_find(&file, "FOLDER2"));
//			printf("Deleted? %d\n", fat16_rmdir(&file));
//		}
//	}

//	printf("Found? %d\n", fat16_find(&file, "FOLDER2"));
//	printf("Deleted? %d\n", fat16_rmdir(&file));

//	if (fat16_mkfile(&file, "banana2.exe"))
//	{
//		fat16_fwrite(&file, "THIS IS A STRING STORED IN A FILE CALLED BANANA!\n", 49);
//	}

//	fat16_find(&file, "banana.exe");
//	char m[49];
//	fat16_fread(&file, m, 49);
//	printf("%.49s\n", m);

	//fat16_mkdir(&file, "FOLDER2");

//	bool found = fat16_find_file(&file, "NEW_FILE.DAT");
//
//	if (found)
//	{
//		fat16_delete_file(&file);
//		fat16_set_file_size(&file, 16000);
//	}

	//printf("Exists? %d\n", fat16_find_file(&file, "nuclear.war"));
	//printf("Size: %d\n", file.size);

	//fat16_set_file_size(&file, 5);

	//fat16_fseek(&file, 40000);
	//fat16_fwrite(&file, "BANANA", 6);

//	FAT16_FILE neu;
//	bool ok = fat16_newfile(&file, &neu, "NEWFILE3.DAT");
//	if (!ok) {
//		printf("FAIL.");
//		return -1;
//	}
//
//	char c = '_';
//	for (uint16_t i = 0; i < 35000; i++) {
//		fat16_fwrite(&neu, &i, 2);
//		fat16_fwrite(&neu, &c, 1);
//	}

	//fat16_fwrite(&neu, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 16);


	//char* text = "KUNDA";
	//fat16_fseek(&file, 30000);
	//fat16_fwrite(&file, text, strlen(text));

	//fat16_fseek(&file, 30000);

	//char boo[5];
	//bool v = fat16_fread(&file, boo, 5);

//	if (!v) {
//		printf("FAIL!\n");
//		return 1;
//	}
//
//	printf("%.5s\n", boo);


//	char ham[20000];
//
//	fat16_find_file(&file, "HAMLET.TXT");
//	fat16_fread(&file, ham, 20000);
//	ham[19999] = 0;
//	printf("%s\n", ham);


	test_close();

	return 0;
}
