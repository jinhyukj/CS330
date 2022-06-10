#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>
#include "devices/disk.h"
#ifdef EFILESYS
#include "filesys/fat.h"
#endif

/* Maximum length of a file name component.
 * This is the traditional UNIX maximum length.
 * After directories are implemented, this maximum length may be
 * retained, but much longer full path names must be allowed. */
#define NAME_MAX 14

struct inode;


/* Edited Code - Jinhyen Kim */

struct dir {
	struct inode *inode; 
	off_t pos;  
    	bool deny_write; 
   	int dupCount; 
};

struct dir_entry {
	disk_sector_t inode_sector;     
	char name[NAME_MAX + 1];    
	bool in_use;   
	bool is_sym;
	char lazy[NAME_MAX + 1];
};

// Project 4-2 
struct dir *getSubdirectory(char ** dirnames, int dircount);
struct dir *current_directory();
void setDirectory(struct dir *dir);
void set_entry_symlink(struct dir*, const char *name, bool);
void set_entry_lazytar(struct dir*, const char *name, const char *tar);

bool lookup (const struct dir *dir, const char *name, struct dir_entry *ep, off_t *ofsp);

/* Edited Code - Jinhyen Kim (Project 4 - Subdirectories and Soft Links) */


/* Opening and closing directories. */
bool dir_create (disk_sector_t sector, size_t entry_cnt);
struct dir *dir_open (struct inode *);
struct dir *dir_open_root (void);
struct dir *dir_reopen (struct dir *);
void dir_close (struct dir *);
struct inode *dir_get_inode (struct dir *);

/* Reading and writing. */
bool dir_lookup (const struct dir *, const char *name, struct inode **);
bool dir_add (struct dir *, const char *name, disk_sector_t);
bool dir_remove (struct dir *, const char *name);
bool dir_readdir (struct dir *, char name[NAME_MAX + 1]);

#endif /* filesys/directory.h */
