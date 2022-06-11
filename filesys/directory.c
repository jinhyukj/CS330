#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "filesys/fat.h"


/* Creates a directory with space for ENTRY_CNT entries in the
 * given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (disk_sector_t sector, size_t entry_cnt) {
	return inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
}

/* Opens and returns the directory for the given INODE, of which
 * it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) {
	struct dir *dir = calloc (1, sizeof *dir);
	if (inode != NULL && dir != NULL) {
		dir->inode = inode;
		dir->pos = 0;

		/* Edited Code - Jinhyen Kim */
		(*(dir)).denyWrite = false;
		(*(dir)).dupNo = 0;
		/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */

		return dir;
	} else {
		inode_close (inode);
		free (dir);
		return NULL;
	}
}

/* Opens the root directory and returns a directory for it.
 * Return true if successful, false on failure. */
struct dir *
dir_open_root (void) {
	
	/* Edited Code - Jinhyen Kim */
	return dir_open (inode_open (cluster_to_sector(ROOT_DIR_SECTOR)));
	/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */

}

/* Opens and returns a new directory for the same inode as DIR.
 * Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) {
	return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) {
	if (dir != NULL) {
		inode_close (dir->inode);
		free (dir);
	}
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) {
	return dir->inode;
}

/* Searches DIR for a file with the given NAME.
 * If successful, returns true, sets *EP to the directory entry
 * if EP is non-null, and sets *OFSP to the byte offset of the
 * directory entry if OFSP is non-null.
 * otherwise, returns false and ignores EP and OFSP. */
bool
lookup (const struct dir *dir, const char *name, struct dir_entry *ep, off_t *ofsp) {
	struct dir_entry e;
	size_t ofs;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
			ofs += sizeof e)
		if (e.in_use && !strcmp (name, e.name)) {
			if (ep != NULL)
				*ep = e;
			if (ofsp != NULL)
				*ofsp = ofs;
			return true;
		}
	return false;
}

/* Searches DIR for a file with the given NAME
 * and returns true if one exists, false otherwise.
 * On success, sets *INODE to an inode for the file, otherwise to
 * a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name, struct inode **inode) 
{
	struct dir_entry e;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	/* Edited Code - Jinhyen Kim */

	if (lookup (dir, name, &e, NULL)) 
	{

		if (strcmp("lazy", e.lazyName))
		{
			dir_lookup(dir_open(inode_open(e.inode_sector)), e.lazyName, inode);

			if (*inode != NULL)
			{
				e.inode_sector = inode_get_inumber(*inode);
				strlcpy (e.lazyName, "lazy", sizeof e.lazyName);
				return *inode != NULL;
			}
			else
			{
				*inode = inode_open (e.inode_sector);
				return *inode != NULL;	
			}

		}
		*inode = inode_open (e.inode_sector);
		return *inode != NULL;

	}
	else
	{
		return false;
	}

	/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */
}

/* Adds a file named NAME to DIR, which must not already contain a
 * file by that name.  The file's inode is in sector
 * INODE_SECTOR.
 * Returns true if successful, false on failure.
 * Fails if NAME is invalid (i.e. too long) or a disk or memory
 * error occurs. */
bool
dir_add (struct dir *dir, const char *name, disk_sector_t inode_sector) {
	struct dir_entry e;
	off_t ofs;
	bool success = false;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	/* Check NAME for validity. */
	if (*name == '\0' || strlen (name) > NAME_MAX)
		return false;

	/* Check that NAME is not in use. */
	if (lookup (dir, name, NULL, NULL))
		goto done;

	/* Set OFS to offset of free slot.
	 * If there are no free slots, then it will be set to the
	 * current end-of-file.

	 * inode_read_at() will only return a short read at end of file.
	 * Otherwise, we'd need to verify that we didn't get a short
	 * read due to something intermittent such as low memory. */
	for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
			ofs += sizeof e)
		if (!e.in_use)
			break;

	/* Write slot. */
	e.in_use = true;
	strlcpy (e.name, name, sizeof e.name);

	/* Edited Code - Jinhyen Kim */
	e.symBool = false;
	strlcpy (e.lazyName, "lazy", sizeof e.lazyName);
	/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */
	
	e.inode_sector = inode_sector;
	success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

done:
	return success;
}

/* Removes any entry for NAME in DIR.
 * Returns true if successful, false on failure,
 * which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) {
	struct dir_entry e;
	struct inode *inode = NULL;
	off_t ofs;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	/* Find directory entry. */
	if (!lookup (dir, name, &e, &ofs))
	{
		inode_close (inode);
		return false;
	}

	/* Open inode. */
	inode = inode_open (e.inode_sector);
	if (inode == NULL) 
	{
		inode_close (inode);
		return false;
	}

	/* Edited Code - Jinhyen Kim */

	if (e.symBool) 
	{
		e.in_use = false;
		return (inode_write_at(dir->inode, &e, sizeof e, ofs) == sizeof e);
	}

	if (checkInodeDir(inode))
	{
		char temp[NAME_MAX + 1];

		struct dir *inodeTemp = dir_open(inode);

		if (dir_readdir(inodeTemp, temp))
		{ 
			(*(dir)).pos = (*(dir)).pos - sizeof(struct dir_entry);
			dir_close(inodeTemp);
			inode_close(inode);
			return false;
		}
		dir_close(inodeTemp);
	}

	/* Erase directory entry. */
	e.in_use = false;
	if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
	{
		inode_close(inode);
		return false;
	}
	
	/* Remove inode. */
	inode_remove (inode);
	inode_close (inode);
	return true;

	/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */

}

/* Reads the next directory entry in DIR and stores the name in
 * NAME.  Returns true if successful, false if the directory
 * contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1]) {
	struct dir_entry e;

	while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) {
		dir->pos += sizeof e;

		/* Edited Code - Jinhyen Kim */

		if ((e.in_use) && (strcmp(e.name, ".") && strcmp(e.name,".."))) 
		{
			strlcpy (name, e.name, NAME_MAX + 1);
			return true;
		}

		/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */

	}
	return false;
}


/* Edited Code - Jinhyen Kim */

struct dir *getDirectory()
{
	return (*(thread_current())).currentDirectory;
}

void setDirectory(struct dir *dir)
{
	(*(thread_current())).currentDirectory = dir;
}

struct dir *getSubdirectory(char **dirList, int dirLevel)
{

	if ((getDirectory()) == NULL) 
	{
		return NULL;
	}

	struct dir *subDir = dir_reopen(getDirectory());

	struct inode *inode = NULL;

	for (int i = 0; i < dirLevel; i++)
	{

		struct dir *prevDir = subDir;

		if (i == 0 && (strcmp(dirList[i],"root") == 0))
		{ 
			subDir = dir_open_root();
			dir_close(prevDir);
			continue;
		}
		else {

			dir_lookup(prevDir, dirList[i], &inode);
		
			if (inode == NULL)
			{	
				return NULL;
			}
		
			subDir = dir_open(inode);
			dir_close(prevDir);

		}
	}

	return subDir;
}

void setIsSymFlag(struct dir* dir, const char *name, bool symBool){

	struct dir_entry dirEntry;
	off_t ofs;

	for (ofs = 0; inode_read_at ((*(dir)).inode, &dirEntry, sizeof dirEntry, ofs) == sizeof dirEntry; ofs += sizeof dirEntry){
		if (dirEntry.in_use && !strcmp (name, dirEntry.name)){
			break;
		}
	}
	dirEntry.symBool = symBool;
	inode_write_at (dir->inode, &dirEntry, sizeof dirEntry, ofs) == sizeof dirEntry;

}

void setSymInfo(struct dir* dir, const char *name, const char *target){

	struct dir_entry dirEntry;
	off_t ofs;

	for (ofs = 0; inode_read_at ((*(dir)).inode, &dirEntry, sizeof dirEntry, ofs) == sizeof dirEntry; ofs += sizeof dirEntry){
		if (dirEntry.in_use && !strcmp (name, dirEntry.name)){
			break;
		}
	}
	strlcpy(dirEntry.lazyName, target, sizeof dirEntry.lazyName);
	inode_write_at ((*(dir)).inode, &dirEntry, sizeof dirEntry, ofs) == sizeof dirEntry;

}

/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */
