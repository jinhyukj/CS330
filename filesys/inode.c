#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#ifdef EFILESYS
#include "filesys/fat.h"
#endif

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Returns the number of sectors to allocate for an inode SIZE
 * bytes long. */
static inline size_t
bytes_to_sectors(off_t size)
{
	return DIV_ROUND_UP(size, DISK_SECTOR_SIZE);
}

/* Returns the disk sector that contains byte offset POS within
 * INODE.
 * Returns -1 if INODE does not contain data for a byte at offset
 * POS. */
static disk_sector_t
byte_to_sector(const struct inode *inode, off_t pos)
{
	ASSERT(inode != NULL);
	if (pos < inode->data.length)
	{

		/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */
		
#ifdef EFILESYS
		cluster_t clst = sector_to_cluster((*(inode)).data.start);
		uint32_t pos_sector = pos / DISK_SECTOR_SIZE;

		for (unsigned i = 0; i < pos_sector; i++)
		{
			clst = fat_get(clst);
			if (clst == 0)
				return -1;
		}

		return cluster_to_sector(clst);

#else
		return inode->data.start + pos / DISK_SECTOR_SIZE;

#endif

		/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */		

	}
	else
		return -1;
}

/* List of open inodes, so that opening a single inode twice
 * returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void inode_init(void)
{
	list_init(&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
 * writes the new inode to sector SECTOR on the file system
 * disk.
 * Returns true if successful.
 * Returns false if memory or disk allocation fails. */
bool inode_create(disk_sector_t sector, off_t length, bool dirFlag)
{
	struct inode_disk *disk_inode = NULL;
	bool success = false;

	ASSERT(length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	 * one sector in size, and you should fix that. */
	//ASSERT(sizeof *disk_inode == DISK_SECTOR_SIZE);

	disk_inode = calloc(1, sizeof *disk_inode);
	if (disk_inode != NULL)
	{
		size_t sectors = bytes_to_sectors(length);
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		/* Edited Code - Jinhyen Kim */
		(*(disk_inode)).dirFlag = dirFlag;
		/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */

#ifdef EFILESYS
		cluster_t clst = sector_to_cluster(sector);
		cluster_t cur = clst;

		if (sectors == 0)
			disk_inode->start = cluster_to_sector(fat_create_chain(cur));
		for (int i = 0; i < sectors; i++)
		{
			cur = fat_create_chain(cur);
			if (cur == 0)
			{
				fat_remove_chain(clst, 0);
				free(disk_inode);
				return false;
			}
			if (i == 0)
			{
				clst = cur;
				disk_inode->start = cluster_to_sector(cur);
			}
		}
		disk_write(filesys_disk, sector, disk_inode);
		if (sectors > 0)
		{
			static char zeros[DISK_SECTOR_SIZE];
			for (int i = 0; i < sectors; i++)
			{
				disk_write(filesys_disk, cluster_to_sector(clst), zeros);
				clst = fat_get(clst);
			}
		}
		success = true;
#else
		if (free_map_allocate(sectors, &disk_inode->start))
		{
			disk_write(filesys_disk, sector, disk_inode);
			if (sectors > 0)
			{
				static char zeros[DISK_SECTOR_SIZE];
				size_t i;

				for (i = 0; i < sectors; i++)
					disk_write(filesys_disk, disk_inode->start + i, zeros);
			}
			success = true;
		}
/*by Jin-Hyuk Jang*/
/*		if (set_num_free_blocks(0) >= sectors)
		{
			disk_write(filesys_disk, sector, disk_inode);
			if (sectors > 0)
			{
				static char zeros[DISK_SECTOR_SIZE];
				size_t i;

				disk_inode->start = fat_create_chain(0);

				disk_write(filesys_disk, sector, disk_inode);
				disk_sector_t prev = disk_inode->start;
				for (i = 1; i < sectors; i++)
				{
					disk_sector_t sector_index = fat_create_chain(prev);
					disk_write(filesys_disk, sector_index, zeros);
					prev = sector_index;
				}
			}
			free(disk_inode);
		}*/
/*by Jin-Hyuk Jang*/
#endif
		free(disk_inode);
	}
	return success;
}

/* Reads an inode from SECTOR
 * and returns a `struct inode' that contains it.
 * Returns a null pointer if memory allocation fails. */
struct inode *
inode_open(disk_sector_t sector)
{
	struct list_elem *e;
	struct inode *inode;

	/* Check whether this inode is already open. */
	for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
		 e = list_next(e))
	{
		inode = list_entry(e, struct inode, elem);
		if (inode->sector == sector)
		{
			inode_reopen(inode);
			return inode;
		}
	}

	/* Allocate memory. */
	inode = malloc(sizeof *inode);
	if (inode == NULL)
		return NULL;

	/* Initialize. */
	list_push_front(&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;
	disk_read(filesys_disk, inode->sector, &inode->data);
	return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen(struct inode *inode)
{
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber(const struct inode *inode)
{
	return inode->sector;
}

/* Closes INODE and writes it to disk.
 * If this was the last reference to INODE, frees its memory.
 * If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode *inode)
{
	/* Ignore null pointer. */
	if (inode == NULL)
		return;

	/* Release resources if this was the last opener. */
	if (--inode->open_cnt == 0)
	{
		/* Remove from inode list and release lock. */
		list_remove(&inode->elem);

		/* Deallocate blocks if removed. */
		if (inode->removed)
		{

			/* Edited Code - Jinhyen Kim */

			#ifdef EFILESYS
			fat_remove_chain(sector_to_cluster((*(inode)).sector), 0);

			#else
			free_map_release (inode->sector, 1);
			free_map_release (inode->data.start, bytes_to_sectors (inode->data.length)); 

			#endif

			/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */

		}

		free(inode);
	}
}

/* Marks INODE to be deleted when it is closed by the last caller who
 * has it open. */
void inode_remove(struct inode *inode)
{
	ASSERT(inode != NULL);
	inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
 * Returns the number of bytes actually read, which may be less
 * than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset)
{
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t *bounce = NULL;

	while (size > 0)
	{
		/* Disk sector to read, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector(inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length(inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE)
		{
			/* Read full sector directly into caller's buffer. */
			disk_read(filesys_disk, sector_idx, buffer + bytes_read);
		}
		else
		{
			/* Read sector into bounce buffer, then partially copy
			 * into caller's buffer. */
			if (bounce == NULL)
			{
				bounce = malloc(DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}
			disk_read(filesys_disk, sector_idx, bounce);
			memcpy(buffer + bytes_read, bounce + sector_ofs, chunk_size);
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;
	}
	free(bounce);

	return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
 * Returns the number of bytes actually written, which may be
 * less than SIZE if end of file is reached or an error occurs.
 * (Normally a write at end of file would extend the inode, but
 * growth is not yet implemented.) */
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size,
					 off_t offset)
{
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t *bounce = NULL;

	if (inode->deny_write_cnt)
		return 0;


	/* Edited Code - Jinhyen Kim */

	bool extendedFlag = false; 
	uint8_t zeroPadding[DISK_SECTOR_SIZE];

	while (size > 0)
	{

		#ifdef EFILESYS
		disk_sector_t sector_idx = byte_to_sector(inode, offset + size);
		while (sector_idx == -1)
		{
			
			off_t inodeLength = inode_length(inode);

			cluster_t endCluster = sector_to_cluster(byte_to_sector(inode, inodeLength - 1));
			cluster_t newCluster;

			if (inodeLength == 0)
			{
				newCluster = endCluster;
			}
			else 
			{
				newCluster = fat_create_chain(endCluster);
			}

			extendedFlag = true;

			if (newCluster != 0) 
			{

				memset (zeroPadding, 0, DISK_SECTOR_SIZE);
				disk_write (filesys_disk, cluster_to_sector(newCluster), zeroPadding); 

				off_t inodeOffset = inodeLength % DISK_SECTOR_SIZE;
				if(inodeOffset != 0)
				{
					((*(inode)).data).length = ((*(inode)).data).length +  DISK_SECTOR_SIZE - inodeOffset; 
					disk_read (filesys_disk, cluster_to_sector(endCluster), zeroPadding);
					memset (zeroPadding + inodeOffset + 1 , 0, DISK_SECTOR_SIZE - inodeOffset);
					disk_write (filesys_disk, cluster_to_sector(endCluster), zeroPadding); 
				}

				((*(inode)).data).length = ((*(inode)).data).length + DISK_SECTOR_SIZE; 
				sector_idx = byte_to_sector(inode, offset + size);
			}
			
		}		
		#endif


		/* Sector to write, starting byte offset within sector. */
		sector_idx = byte_to_sector(inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length(inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left;
		if (inode_left < sector_left)
		{
			min_left = inode_left;
		}
		else
		{
			min_left = sector_left;
		}

		/* Number of bytes to actually write into this sector. */
		int chunk_size;


		if (size < min_left)
		{
			chunk_size = size;
		}
		else
		{
			chunk_size = min_left;
		} 

		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE)
		{
			/* Write full sector directly to disk. */
			disk_write(filesys_disk, sector_idx, buffer + bytes_written);
		}
		else
		{
			/* We need a bounce buffer. */
			if (bounce == NULL)
			{
				bounce = malloc(DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}

			/* If the sector contains data before or after the chunk
			   we're writing, then we need to read in the sector
			   first.  Otherwise we start with a sector of all zeros. */
			if (sector_ofs > 0 || chunk_size < sector_left)
				disk_read(filesys_disk, sector_idx, bounce);
			else
				memset(bounce, 0, DISK_SECTOR_SIZE);
			memcpy(bounce + sector_ofs, buffer + bytes_written, chunk_size);
			disk_write(filesys_disk, sector_idx, bounce);
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;

		sector_idx = byte_to_sector (inode, offset);
	}

	if (extendedFlag == true)
	{
		((*(inode)).data).length = offset;
	}

	free(bounce);
	disk_write (filesys_disk, inode->sector, &inode->data); 

	return bytes_written;

	/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */

}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void inode_deny_write(struct inode *inode)
{
	inode->deny_write_cnt++;
	ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
 * Must be called once by each inode opener who has called
 * inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write(struct inode *inode)
{
	ASSERT(inode->deny_write_cnt > 0);
	ASSERT(inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode *inode)
{
	return inode->data.length;
}

/* Edited Code - Jinhyen Kim */

bool checkInodeDir (struct inode *inode) {
	return ((*(inode)).data).dirFlag;
}

/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */
