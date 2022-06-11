#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format(void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void filesys_init(bool format)
{
	filesys_disk = disk_get(0, 1);
	if (filesys_disk == NULL)
		PANIC("hd0:1 (hdb) not present, file system initialization failed");

	inode_init();

	lock_init(&fileSysLock);

#ifdef EFILESYS
	fat_init();

	if (format)
		do_format();

	fat_open();
	initFatBitmap();
#else
	/* Original FS */
	free_map_init();

	if (format)
		do_format();

	free_map_open();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void filesys_done(void)
{
	/* Original FS */
#ifdef EFILESYS
	fat_close();
#else
	free_map_close();
#endif
}

/* Edited Code - Jinhyen Kim */

void freePath(struct path* path){

	free((*(path)).dirList);
	free((*(path)).pathTemp);
	free(path);

}

/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */


/* Edited Code - Jinhyen Kim */

struct path* parsePath (const char *oldName){

	struct path* path = malloc(sizeof(struct path));
	char **buf = calloc(sizeof(char *), 30); 
	int i = 0;

	int pathLen = strlen(oldName) + 1;
	char* newName = malloc(pathLen);
	strlcpy(newName, oldName, pathLen);

	(*(path)).pathTemp = newName; 

	if (newName[0] == '/'){ 
		buf[0] = "root";
		i++;
	}

	char *token, *save_ptr;
	token = strtok_r(newName, "/", &save_ptr);
	while (token != NULL)
	{

		if(strlen(token) > NAME_MAX){
			(*(path)).dirLevel = -1; 
			return path;
		}

		buf[i] = token;
		token = strtok_r(NULL, "/", &save_ptr);
		i++;
	}
	(*(path)).dirList = buf; 
	(*(path)).dirLevel = i-1;
	(*(path)).file = buf[i-1]; 
	return path;
}

/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */




/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */

/* Edited Code - Jinhyen Kim */

bool filesys_create(const char *name, off_t initial_size)
{

	lock_acquire(&fileSysLock);

	struct path* path = parsePath(name);
	struct dir* dir = getSubdirectory((*(path)).dirList, (*(path)).dirLevel);
	struct inode *inode = NULL; 

	if (((*(path)).dirLevel == -1) || (dir == NULL) || (dir_lookup(dir, (*(path)).file, &inode))) 
	{
		dir_close(dir);
		freePath(path);
		lock_release(&fileSysLock);
		return false;
	}

	disk_sector_t inode_sector = 0;

	bool success;

	#ifdef EFILESYS
	cluster_t clst = fat_create_chain(0);
	if (clst == 0)
	{ 
		dir_close(dir);
		freePath(path);
		lock_release(&fileSysLock);
		return false;
	}
	inode_sector = cluster_to_sector(clst);

	success = (dir != NULL			
			&& inode_create (inode_sector, initial_size, false)
			&& dir_add (dir, (*(path)).file, inode_sector));

	if (!success)
		fat_remove_chain (inode_sector, 0);

	#else
	success = (dir != NULL
			&& free_map_allocate (1, &inode_sector)
			&& inode_create (inode_sector, initial_size, false)
			&& dir_add (dir, name, inode_sector));

	if (!success && inode_sector != 0)
		free_map_release (inode_sector, 1);

	#endif

	dir_close (dir);
	freePath(path);
	lock_release(&fileSysLock);
	return success;

}

/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */

/* Edited Code - Jinhyen Kim */

struct file *
filesys_open(const char *name)
{

	lock_acquire(&fileSysLock);

	struct path* path = parsePath(name);
	struct dir* dir = getSubdirectory((*(path)).dirList, (*(path)).dirLevel);

	if (((*(path)).dirLevel == -1) || (dir == NULL))
	{ 
		dir_close(dir);
		freePath(path);
		lock_release(&fileSysLock);
		return NULL;
	}


	if ((*(path)).file == "root")
	{ 
		lock_release(&fileSysLock);
		return file_open(inode_open(cluster_to_sector(1)));
	}
	else
	{
		struct inode *inode = NULL;
		dir_lookup(dir, (*(path)).file, &inode);

		struct file *file = file_open(inode);
		dir_close(dir);
		freePath(path);
		lock_release(&fileSysLock);

		return file;
	}


}

/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */

/* Edited Code - Jinhyen Kim */

bool filesys_remove(const char *name)
{

	lock_acquire(&fileSysLock);

	struct path* path = parsePath(name);
	struct dir* dir = getSubdirectory((*(path)).dirList, (*(path)).dirLevel);

	if (((*(path)).dirLevel == -1) || (dir == NULL)) 
	{
		dir_close(dir);
		freePath(path);
		lock_release(&fileSysLock);
		return false;
	}

	struct inode *inode = NULL; 

	dir_lookup(dir, (*(path)).file, &inode);

	if(inode == NULL)
	{
		dir_close(dir);
		freePath(path);
		lock_release(&fileSysLock);
		return false;
	}

	struct dir *curDir = getDirectory();

	if((*(curDir)).inode == inode)
		setDirectory(NULL); 

	bool success = ((dir != NULL) && (dir_remove (dir, (*(path)).file)));


	dir_close (dir);
	freePath(path);
	lock_release(&fileSysLock);
	return success;


}

/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */

/* Formats the file system. */
static void
do_format(void)
{
	printf("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create();

	disk_sector_t rootsect = cluster_to_sector(ROOT_DIR_CLUSTER);
	if (!dir_create(rootsect, DISK_SECTOR_SIZE/sizeof (struct dir_entry)))
		PANIC ("root directory creation failed");
	struct dir* rootdir = dir_open(inode_open(rootsect));
	dir_add(rootdir, ".", rootsect);
	dir_close(rootdir);
	fat_close();

#else
	free_map_create();
	if (!dir_create(ROOT_DIR_SECTOR, 16))
		PANIC("root directory creation failed");
	free_map_close();
#endif

	printf("done.\n");
}
