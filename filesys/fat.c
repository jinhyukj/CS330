#include "filesys/fat.h"
#include "devices/disk.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <stdio.h>
#include <string.h>

/* Should be less than DISK_SECTOR_SIZE */
struct fat_boot
{
	unsigned int magic;
	unsigned int sectors_per_cluster; /* Fixed to 1 */
	unsigned int total_sectors;
	unsigned int fat_start;
	unsigned int fat_sectors; /* Size of FAT in sectors. */
	unsigned int root_dir_cluster;

	/*by Jin-Hyuk Jang*/
	unsigned int num_free_blocks;
	/*by Jin-Hyuk Jang*/
};

/* FAT FS */
struct fat_fs
{
	struct fat_boot bs;
	unsigned int *fat;
	unsigned int fat_length;
	disk_sector_t data_start;
	cluster_t last_clst;
	struct lock write_lock;

	/*by Jin-Hyuk Jang*/
	unsigned int num_free_blocks;
	/*by Jin-Hyuk Jang*/
};

static struct fat_fs *fat_fs;

void fat_boot_create(void);
void fat_fs_init(void);

void fat_init(void)
{
	fat_fs = calloc(1, sizeof(struct fat_fs));
	if (fat_fs == NULL)
		PANIC("FAT init failed");

	// Read boot sector from the disk
	unsigned int *bounce = malloc(DISK_SECTOR_SIZE);
	if (bounce == NULL)
		PANIC("FAT init failed");
	disk_read(filesys_disk, FAT_BOOT_SECTOR, bounce);
	memcpy(&fat_fs->bs, bounce, sizeof(fat_fs->bs));
	free(bounce);

	// Extract FAT info
	if (fat_fs->bs.magic != FAT_MAGIC)
		fat_boot_create();
	fat_fs_init();
}

void fat_open(void)
{
	fat_fs->fat = calloc(fat_fs->fat_length, sizeof(cluster_t));
	if (fat_fs->fat == NULL)
		PANIC("FAT load failed");

	// Load FAT directly from the disk
	uint8_t *buffer = (uint8_t *)fat_fs->fat;
	off_t bytes_read = 0;
	off_t bytes_left = sizeof(fat_fs->fat);
	const off_t fat_size_in_bytes = fat_fs->fat_length * sizeof(cluster_t);
	for (unsigned i = 0; i < fat_fs->bs.fat_sectors; i++)
	{
		bytes_left = fat_size_in_bytes - bytes_read;
		if (bytes_left >= DISK_SECTOR_SIZE)
		{
			disk_read(filesys_disk, fat_fs->bs.fat_start + i,
					  buffer + bytes_read);
			bytes_read += DISK_SECTOR_SIZE;
		}
		else
		{
			uint8_t *bounce = malloc(DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC("FAT load failed");
			disk_read(filesys_disk, fat_fs->bs.fat_start + i, bounce);
			memcpy(buffer + bytes_read, bounce, bytes_left);
			bytes_read += bytes_left;
			free(bounce);
		}
	}
}

void fat_close(void)
{
	// Write FAT boot sector
	uint8_t *bounce = calloc(1, DISK_SECTOR_SIZE);
	if (bounce == NULL)
		PANIC("FAT close failed");
	memcpy(bounce, &fat_fs->bs, sizeof(fat_fs->bs));
	disk_write(filesys_disk, FAT_BOOT_SECTOR, bounce);
	free(bounce);

	// Write FAT directly to the disk
	uint8_t *buffer = (uint8_t *)fat_fs->fat;
	off_t bytes_wrote = 0;
	off_t bytes_left = sizeof(fat_fs->fat);
	const off_t fat_size_in_bytes = fat_fs->fat_length * sizeof(cluster_t);
	for (unsigned i = 0; i < fat_fs->bs.fat_sectors; i++)
	{
		bytes_left = fat_size_in_bytes - bytes_wrote;
		if (bytes_left >= DISK_SECTOR_SIZE)
		{
			disk_write(filesys_disk, fat_fs->bs.fat_start + i,
					   buffer + bytes_wrote);
			bytes_wrote += DISK_SECTOR_SIZE;
		}
		else
		{
			bounce = calloc(1, DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC("FAT close failed");
			memcpy(bounce, buffer + bytes_wrote, bytes_left);
			disk_write(filesys_disk, fat_fs->bs.fat_start + i, bounce);
			bytes_wrote += bytes_left;
			free(bounce);
		}
	}
}

void fat_create(void)
{
	// Create FAT boot
	fat_boot_create();
	fat_fs_init();

	// Create FAT table
	fat_fs->fat = calloc(fat_fs->fat_length, sizeof(cluster_t));
	if (fat_fs->fat == NULL)
		PANIC("FAT creation failed");

	// Set up ROOT_DIR_CLST
	fat_put(ROOT_DIR_CLUSTER, EOChain);

	// Fill up ROOT_DIR_CLUSTER region with 0
	uint8_t *buf = calloc(1, DISK_SECTOR_SIZE);
	if (buf == NULL)
		PANIC("FAT create failed due to OOM");
	disk_write(filesys_disk, cluster_to_sector(ROOT_DIR_CLUSTER), buf);
	free(buf);

	/*by Jin-Hyuk Jang
	create linked list for empty sectors of fat*/
	uint32_t i = 0;
	for (i; i < fat_fs->bs.total_sectors - 1; i++)
	{
		fat_fs->fat[i] = i + 1;
	}

	fat_put(0, EOChain);

	fat_put(1, EOChain);

	fat_put(fat_fs->bs.fat_start + fat_fs->bs.fat_sectors - 1, EOChain);

	fat_put(fat_fs->bs.total_sectors - 1, EOChain);
	/*by jin-Hyuk Jang*/
}

void fat_boot_create(void)
{
	unsigned int fat_sectors =
		(disk_size(filesys_disk) - 1) / (DISK_SECTOR_SIZE / sizeof(cluster_t) * SECTORS_PER_CLUSTER + 1) + 1;
	fat_fs->bs = (struct fat_boot){
		.magic = FAT_MAGIC,
		.sectors_per_cluster = SECTORS_PER_CLUSTER,
		.total_sectors = disk_size(filesys_disk),
		.fat_start = 1,
		.fat_sectors = fat_sectors,
		.root_dir_cluster = ROOT_DIR_CLUSTER,

		/*by Jin-Hyuk Jang*/
		.num_free_blocks = disk_size(filesys_disk) - fat_sectors - 4,
		/*by Jin-Hyuk Jang*/
	};
}

void fat_fs_init(void)
{
	/* TODO: Your code goes here. */
	fat_fs->fat = NULL;
	fat_fs->fat_length = fat_fs->bs.fat_sectors;
	fat_fs->data_start = fat_fs->bs.fat_start + fat_fs->bs.fat_sectors;
	fat_fs->num_free_blocks = fat_fs->bs.num_free_blocks;
	lock_init(&fat_fs->write_lock);
}

/*----------------------------------------------------------------------------*/
/* FAT handling                                                               */
/*----------------------------------------------------------------------------*/

/*by Jin-Hyuk Jang*/
cluster_t get_free_cluster(void)
{
	cluster_t clst = fat_get(fat_fs->data_start);
	if (clst != fat_fs->bs.total_sectors - 1)
	{
		cluster_t next_clst = fat_get(clst);
		fat_put(fat_fs->data_start, next_clst);
		fat_put(clst, EOChain);
		fat_fs->num_free_blocks -= 1;
		return clst;
	}
	return -1;
}
/*by Jin-Hyuk Jang*/

/* Add a cluster to the chain.
 * If CLST is 0, start a new chain.
 * Returns 0 if fails to allocate a new cluster. */
cluster_t
fat_create_chain(cluster_t clst)
{
	/* TODO: Your code goes here. */
	cluster_t free_clst = get_free_cluster();

	if (clst != -1)
	{
		if (clst != 0)
		{
			fat_put(clst, free_clst);
		}
		return free_clst;
	}
	return 0;
}

/* Remove the chain of clusters starting from CLST.
 * If PCLST is 0, assume CLST as the start of the chain. */
void fat_remove_chain(cluster_t clst, cluster_t pclst)
{
	/* TODO: Your code goes here. */
	if (pclst != 0)
		fat_put(pclst, EOChain);

	cluster_t head = fat_fs->data_start;
	cluster_t second = fat_get(head);

	cluster_t next_clst = fat_get(clst);

	fat_put(clst, second);
	fat_put(head, clst);
	fat_fs->num_free_blocks += 1;

	while (next_clst != EOChain)
	{
		clst = fat_get(next_clst);
		second = fat_get(head);
		fat_put(next_clst, second);
		fat_put(head, next_clst);
		fat_fs->num_free_blocks += 1;
	}
}

/* Update a value in the FAT table. */
void fat_put(cluster_t clst, cluster_t val)
{
	/* TODO: Your code goes here. */
	fat_fs->fat[clst] = val;
}

/* Fetch a value in the FAT table. */
cluster_t
fat_get(cluster_t clst)
{
	/* TODO: Your code goes here. */
	return fat_fs->fat[clst];
}

/* Covert a cluster # to a sector number. */
disk_sector_t
cluster_to_sector(cluster_t clst)
{
	/* TODO: Your code goes here. */
	return clst;
}

/*set num_free_blocks of fat_fs*/
size_t set_num_free_blocks(uint32_t n)
{
	fat_fs->num_free_blocks += n;
	return fat_fs->num_free_blocks;
}