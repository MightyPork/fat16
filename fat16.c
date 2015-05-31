#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "fat16.h"


char* fat16_volume_label(const FAT16* fat, char* str)
{
	FAT16_FILE first;
	fat16_open_root(fat, &first);

	if (first.type == FT_LABEL) {
		return fat16_display_name(&first, str);
	}

	// find where spaces end
	uint8_t j = 10;
	for (; j >= 0; j--)
	{
		if (fat->bs.volume_label[j] != ' ') break;
	}

	// copy all until spaces
	uint8_t i;
	for (i = 0; i <= j; i++)
	{
		str[i] = fat->bs.volume_label[i];
	}

	str[i] = 0; // ender

	return str;
}


/**
 * Resolve a file name, trim spaces and add null terminator.
 * Returns the passed char*, or NULL on error.
 */
char* fat16_display_name(const FAT16_FILE* file, char* str)
{
	// Cannot get name for special files
	if (file->type == FT_NONE ||        // not-yet-used directory location
		file->type == FT_DELETED ||     // deleted file entry
		file->attribs == 0x0F)          // long name special entry (system, hidden)
		return NULL;

	// find first non-space
	uint8_t j = 7;
	for (; j >= 0; j--)
	{
		if (file->name[j] != ' ') break;
	}

	// j ... last no-space char

	uint8_t i;
	for (i = 0; i <= j; i++)
	{
		str[i] = file->name[i];
	}


	// directory entry, no extension
	if (file->type == FT_SUBDIR || file->type == FT_SELF || file->type == FT_PARENT)
	{
		str[i] = 0; // end of string
		return str;
	}


	// add a dot
	if (file->type != FT_LABEL) // volume label has no dot!
		str[i++] = '.';

	// Add extension chars
	for (j = 0; j < 3; j++, i++)
	{
		const char c = file->ext[j];
		if (c == ' ') break;
		str[i] = c;
	}

	str[i] = 0; // end of string

	return str;
}


/** Read boot sector from given address */
void _fat16_read_bs(const BLOCKDEV* dev, Fat16BootSector* info, const uint32_t addr);

/**
 * Find absolute address of first BootSector.
 * Returns 0 on failure.
 */
uint32_t _fat16_find_bs(const BLOCKDEV* dev);


/** Get cluster's starting address */
uint32_t _fat16_clu_start(const FAT16* fat, const uint16_t cluster);


/** Find following cluster using FAT for jumps */
uint16_t _fat16_next_clu(const FAT16* fat, uint16_t cluster);


/** Find relative address in a file, using FAT for cluster lookup */
uint32_t _fat16_clu_add(const FAT16* fat, uint16_t cluster, uint32_t addr);


/** Read a file entry from directory (dir starting cluster, entry number) */
void _fat16_fopen(const FAT16* fat, FAT16_FILE* file, const uint16_t dir_cluster, const uint16_t num);




/**
 * Find absolute address of first boot sector.
 * Returns 0 on failure.
 */
uint32_t _fat16_find_bs(const BLOCKDEV* dev)
{
	//	Reference structure:
	//
	//	typedef struct __attribute__((packed)) {
	//		uint8_t first_byte;
	//		uint8_t start_chs[3];
	//		uint8_t partition_type;
	//		uint8_t end_chs[3];
	//		uint32_t start_sector;
	//		uint32_t length_sectors;
	//	} PartitionTable;

	uint16_t addr = 0x1BE + 4; // fourth byte of structure is the type.
	uint32_t tmp = 0;
	uint16_t tmp2;

	for (uint8_t i = 0; i < 4; i++, addr += 16)
	{
		// Read partition type
		dev->aread(&tmp, 1, addr);

		// Check if type is valid
		if (tmp == 4 || tmp == 6 || tmp == 14)
		{
			// read MBR address
			dev->rseek(3);// skip 3 bytes
			dev->read(&tmp, 4);

			tmp = tmp << 9; // multiply address by 512 (sector size)

			// Verify that the boot sector has a valid signature mark
			dev->aread(&tmp2, 2, tmp + 510);
			if (tmp2 != 0xAA55) continue; // continue to next entry

			// return absolute MBR address
			return tmp;
		}
	}

	return 0;
}


void _fat16_read_bs(const BLOCKDEV* dev, Fat16BootSector* info, const uint32_t addr)
{
	dev->seek(addr + 13); // skip 13

	dev->read(&(info->sectors_per_cluster), 6); // spc, rs, nf, re

	info->total_sectors = 0;
	dev->read(&(info->total_sectors), 2); // short sectors

	dev->rseek(1); // md

	dev->read(&(info->fat_size_sectors), 2);

	dev->rseek(8); // spt, noh, hs

	// read or skip long sectors field
	if (info->total_sectors == 0)
	{
		dev->read(&(info->total_sectors), 4);
	} else
	{
		dev->rseek(4); // tsl
	}

	dev->rseek(7); // dn, ch, bs, vi

	dev->read(&(info->volume_label), 11);
}


/** Initialize a FAT16 handle */
void fat16_init(const BLOCKDEV* dev, FAT16* fat)
{
	const uint32_t bs_a = _fat16_find_bs(dev);
	fat->dev = dev;
	_fat16_read_bs(dev, &(fat->bs), bs_a);
	fat->fat_addr = bs_a + (fat->bs.reserved_sectors * 512);
	fat->rd_addr = bs_a + (fat->bs.reserved_sectors + fat->bs.fat_size_sectors * fat->bs.num_fats) * 512;
	fat->data_addr = fat->rd_addr + (fat->bs.root_entries * 32); // entry is 32B long

	fat->bs.bytes_per_cluster = (fat->bs.sectors_per_cluster * 512);
}


/** Get cluster starting address */
uint32_t _fat16_clu_start(const FAT16* fat, const uint16_t cluster)
{
	if (cluster < 2) return fat->rd_addr;
	return fat->data_addr + (cluster - 2) * fat->bs.bytes_per_cluster;
}


uint16_t _fat16_next_clu(const FAT16* fat, uint16_t cluster)
{
	fat->dev->aread(&cluster, 2, fat->fat_addr + (cluster * 2));
	return cluster;
}


/** Find file-relative address in fat table */
uint32_t _fat16_clu_add(const FAT16* fat, uint16_t cluster, uint32_t addr)
{
	while (addr >= fat->bs.bytes_per_cluster)
	{
		cluster = _fat16_next_clu(fat, cluster);
		if (cluster == 0xFFFF) return 0xFFFF; // fail
		addr -= fat->bs.bytes_per_cluster;
	}

	return _fat16_clu_start(fat, cluster) + addr;
}


/** Move file cursor to a position relative to file start */
bool fat16_fseek(FAT16_FILE* file, uint32_t addr)
{
	// Clamp.
	if (addr > file->size)
		return false;

	// Store as rel
	file->cur_rel = addr;

	// Rewind and resolve abs, clu, ofs
	file->cur_clu = file->clu_start;

	while (addr >= file->fat->bs.bytes_per_cluster)
	{
		file->cur_clu = _fat16_next_clu(file->fat, file->cur_clu);
		addr -= file->fat->bs.bytes_per_cluster;
	}

	file->cur_abs = _fat16_clu_start(file->fat, file->cur_clu) + addr;
	file->cur_ofs = addr;

	return true;
}


/**
 * Read a file entry
 *
 * dir_cluster ... directory start cluster
 * num ... entry number in the directory
 */
void _fat16_fopen(const FAT16* fat, FAT16_FILE* file, const uint16_t dir_cluster, const uint16_t num)
{
	// Resolve starting address
	uint32_t addr;
	if (dir_cluster == 0)
	{
		addr = _fat16_clu_start(fat, dir_cluster) + num * 32; // root directory, max 512 entries.
	} else
	{
		addr = _fat16_clu_add(fat, dir_cluster, num * 32); // cluster + N (wrapping to next cluster if needed)
	}

	fat->dev->aread(file, 12, addr);
	fat->dev->rseek(14); // skip 14 bytes
	fat->dev->read(((void*)file) + 12, 6); // read remaining bytes

	file->clu = dir_cluster;
	file->num = num;

	// Resolve filename & type

	file->type = FT_FILE;

	switch(file->name[0])
	{
		case 0x00:
			file->type = FT_NONE;
			return;

		case 0xE5:
			file->type = FT_DELETED;
			return;

		case 0x05: // Starting with 0xE5
			file->type = FT_FILE;
			file->name[0] = 0xE5; // convert to the real character
			break;

		case 0x2E:
			if (file->name[1] == 0x2E)
			{
				// ".." directory
				file->type = FT_PARENT;
			} else
			{
				// "." directory
				file->type = FT_SELF;
			}
			break;

		default:
			file->type = FT_FILE;
	}

	// handle subdir, label
	if (file->attribs & FA_DIR && file->type == FT_FILE)
	{
		file->type = FT_SUBDIR;
	} else
	if (file->attribs == FA_LABEL)
	{
		file->type = FT_LABEL; // volume label special file
	} else
	if (file->attribs == 0x0F)
	{
		file->type = FT_LFN; // long name special file, can be safely ignored
	}

	// add a FAT pointer
	file->fat = fat;

	// Init cursors
	fat16_fseek(file, 0);
}


/**
 * Check if file is a valid entry (to be shown)
 */
bool fat16_is_file_valid(const FAT16_FILE* file)
{
	switch (file->type) {
		case FT_FILE:
		case FT_SUBDIR:
		case FT_SELF:
		case FT_PARENT:
			return true;

		default:
			return false;
	}
}



#define MIN(a, b) (((a) < (b)) ? (a) : (b))

bool fat16_fread(FAT16_FILE* file, void* target, uint32_t len)
{
	if (file->cur_abs == 0xFFFF)
		return false; // file at the end already

	if (file->cur_rel + len > file->size)
		return false; // attempt to read outside file size

	while (len > 0 && file->cur_rel < file->size)
	{
		uint16_t chunk = MIN(file->size - file->cur_rel, MIN(file->fat->bs.bytes_per_cluster - file->cur_ofs, len));

		file->fat->dev->aread(target, chunk, file->cur_abs);

		file->cur_abs += chunk;
		file->cur_rel += chunk;
		file->cur_ofs += chunk;

		target += chunk;

		if (file->cur_ofs >= file->fat->bs.bytes_per_cluster)
		{
			file->cur_clu = _fat16_next_clu(file->fat, file->cur_clu);
			file->cur_abs = _fat16_clu_start(file->fat, file->cur_clu);
			file->cur_ofs = 0;
		}

		len -= chunk;
	}

	return true;
}


/** Open next file in the directory */
bool fat16_next(FAT16_FILE* file)
{
	if (file->clu == 0 && file->num >= file->fat->bs.root_entries)
		return false; // attempt to read outside root directory.

	uint32_t addr = _fat16_clu_add(file->fat, file->clu, (file->num + 1) * 32);
	if (addr == 0xFFFF)
		return false; // next file is out of the directory cluster

	// read first byte of the file entry; if zero, can't read (file is NONE)
	// FIXME this may be a problem when creating a new file...
	uint8_t x;
	file->fat->dev->aread(&x, 1, addr);
	if (x == 0)
		return false;

	_fat16_fopen(file->fat, file, file->clu, file->num+1);

/*	// Skip bad files
	if (!fat16_is_file_valid(file))
		fat16_next(file);*/

	return true;
}


/** Open previous file in the directory */
bool fat16_prev(FAT16_FILE* file)
{
	if (file->num == 0)
		return false; // first file already

	_fat16_fopen(file->fat, file, file->clu, file->num-1);

/*	// Skip bad files
	if (!fat16_is_file_valid(file))
		fat16_prev(file);*/

	return true;
}


/** Open a directory */
bool fat16_opendir(FAT16_FILE* file)
{
	// Don't open non-dirs and "." directory.
	if (!(file->attribs & FA_DIR) || file->type == FT_SELF)
		return false;

	_fat16_fopen(file->fat, file, file->clu_start, 0);
	return true;
}


void fat16_open_root(const FAT16* fat, FAT16_FILE* file)
{
	_fat16_fopen(fat, file, 0, 0);
}
