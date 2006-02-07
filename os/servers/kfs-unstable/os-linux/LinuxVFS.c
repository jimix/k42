/*
 * K42 File System
 *
 * This is a wrapper around K42's FileSystem Server's Objects,
 * so that they connect to Linux's VFS layer.
 *
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 * 
 * $Id: LinuxVFS.c,v 1.1 2004/02/11 23:03:59 lbsoares Exp $
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#include <asm/uaccess.h>

#include "LinuxVFS.h"
#include "defines.H"

#define KFS_NAME_LEN        512

#define OS_SECTOR_SIZE      512
#define OS_BLOCK_SIZE       PAGE_SIZE

/* some random number */
#define KFS_SUPER_MAGIC	    0x42

static struct super_operations kfs_sops;
static struct file_operations kfs_file_file_operations;
static struct file_operations kfs_dir_file_operations;
static struct inode_operations kfs_dir_inode_operations;
static struct inode_operations kfs_file_inode_operations;
static struct inode_operations kfs_symlink_inode_operations;
static struct address_space_operations kfs_file_inode_aops;

//#include "../KFSDebug.H"
#define KFS_DPRINTF(mask, strargs...) // printk(KERN_ERR strargs)
#define err_printf(stdargs...) printk(KERN_ERR stdargs)

static long int total_mem = 0, num_alloc = 0, num_dealloc = 0;
static long int total_kfs_reads = 0, total_reads = 0, total_kfs_writes = 0, total_writes = 0;
static long int writes_page_nomap = 0, writes_page_map = 0, writes_buffer = 0;
static long int reads_page_nomap = 0, reads_page_map = 0, reads_buffer_dirty = 0, reads_buffer_clean = 0;
static long int reads_page_map_no_bh = 0, reads_page_map_buffer_uptodate = 0, reads_page_map_page_uptodate = 0;

static long int block_cache_allocs = 0, block_cache_frees = 0;

static int block_cache_entry_size;
static kmem_cache_t * block_cachep;


static int 
kfs_statfs(struct super_block *sb, struct statfs *buf)
{
	int rc = 0;

	KFS_DPRINTF(DebugMask::LINUX, "(kfs_statfs)\n");
	if ((rc = Kfs_getStatfs(sb->s_root->d_inode->u.generic_ip, buf)))
		return rc;

	buf->f_type = KFS_SUPER_MAGIC; /* type of filesystem (see below) */
	buf->f_bavail = buf->f_bfree;  /* free blocks avail to non-superuser */
	buf->f_namelen = 1024;         /* maximum length of filenames */

	printk("KFS memory usage:\n\tTotal memory=%ld\n\tNum allocations=%ld\n"
	       "\tNum deallocations=%ld\n"
	       "\tTotal reads=%ld (kfs=%ld page-nomap=%ld, page-map=%ld (%ld, %ld - %ld), b_dirty=%ld, b_clean=%ld)\n"
	       "\tTotal writes=%ld (kfs=%ld page-nomap=%ld, page-map=%ld, b=%ld)\n"
	       "\tTotal block allocs=%ld, block frees=%ld\n",
	       total_mem, num_alloc, num_dealloc, 
	       total_reads, total_kfs_reads, reads_page_nomap, reads_page_map, reads_page_map_no_bh,
	       reads_page_map_page_uptodate, reads_page_map_buffer_uptodate, reads_buffer_dirty, reads_buffer_clean,
	       total_writes, total_kfs_writes, writes_page_nomap, writes_page_map, writes_buffer,
	       block_cache_allocs, block_cache_frees);

	return rc;
}

static void
kfs_set_inode_mode(struct inode * inode)
{
	switch (inode->i_mode & S_IFMT) {
	case S_IFREG:
		inode->i_fop = &kfs_file_file_operations;
		inode->i_op = &kfs_file_inode_operations;
		inode->i_mapping->a_ops = &kfs_file_inode_aops;
		inode->i_mapping->host = inode;
		break;
	case S_IFDIR:
		inode->i_op = &kfs_dir_inode_operations;
		inode->i_fop = &kfs_dir_file_operations;
		break;
	case S_IFLNK:
		inode->i_op = &kfs_symlink_inode_operations;
		inode->i_mapping->a_ops = &kfs_file_inode_aops;
		inode->i_mapping->host = inode;
		break;
	default:
		init_special_inode(inode, inode->i_mode, inode->i_sb->s_dev);
		break;
	}
}

static void
kfs_get_status(struct inode *inode)
{
  	struct stat64 stat;
	int new = 0;
	
	KFS_DPRINTF(DebugMask::LINUX, "(kfs_get_status) IP=0x%p i_ino=%ld\n",
		    inode->u.generic_ip, inode->i_ino);
	
	// fresh, new inode! Get a FSFileKFS reference and put into generic_ip
	if (!inode->u.generic_ip) {
		inode->u.generic_ip = Kfs_getToken(inode->i_ino, inode);
		new = 1;
	}

	Kfs_getStatus(inode->u.generic_ip, &stat);

	if (!inode->i_ino) {
		inode->i_ino = stat.st_ino;
	}

	if (new) {
		// These only have to initialized once.
		inode->i_dev     = stat.st_dev;
		inode->i_rdev    = stat.st_dev;
		inode->i_blksize = stat.st_blksize; // PAGE_SIZE ??
	}

	inode->i_mode    = stat.st_mode;
	inode->i_nlink   = stat.st_nlink;
	inode->i_uid     = stat.st_uid;
	inode->i_gid     = stat.st_gid;
	inode->i_size    = stat.st_size;
	inode->i_blocks  = stat.st_blocks;
	inode->i_atime   = stat.st_atime;
	inode->i_mtime   = stat.st_mtime;
	inode->i_ctime   = stat.st_ctime;
}

static inline void
kfs_read_inode(struct inode *inode)
{
	KFS_DPRINTF(DebugMask::LINUX, "(kfs_read_inode) IP=0x%p i_ino=%ld\n",
		    inode->u.generic_ip, inode->i_ino);

	kfs_get_status(inode);
	kfs_set_inode_mode(inode);
}

static struct dentry * 
kfs_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode * inode;
	ino_t ino;

	KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_lookup): dentry=%s\n", dentry->d_name.name);

	ino = Kfs_lookup(dir->u.generic_ip, dentry->d_name.name,
			 dentry->d_name.len);
	inode = NULL;
	if (ino) {
		inode = iget(dir->i_sb, ino);
		if (!inode) {
			return ERR_PTR(-EACCES);
		}
        }
        d_add(dentry, inode);
        return NULL;
}

static int
kfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry)
{
	struct inode * inode = old_dentry->d_inode;
        int error;

	KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_link): old_dentry=%s, new_dentry=%s\n",
		    old_dentry->d_name.name, dentry->d_name.name);

	error = Kfs_link(inode->u.generic_ip, dir->u.generic_ip, 
			 (char *)dentry->d_name.name, dentry->d_name.len);

        if (!error) {
		kfs_read_inode(inode);
//		kfs_read_inode(dir);

		atomic_inc(&inode->i_count);
		d_instantiate(dentry, inode);

		mark_inode_dirty(inode);
//		mark_inode_dirty(dir);
		dir->i_sb->s_dirt = 1;

	}
	else {
	        KFS_DPRINTF(DebugMask::LINUX,
			    "(kfs_link) got error=%d\n", error);
	}

        return error;    
}

static int
kfs_readdir(struct file * filp, void * dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct dirent64 *ptr;
	struct dirent64 *buf = (struct dirent64 *)__get_free_page(GFP_KERNEL);
	loff_t cookie, len;
	
	KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_readdir) ini: %s, IP=0x%p, cookie=%lld\n", 
		    filp->f_dentry->d_name.name, 
		    (void *)filp->f_dentry->d_inode->u.generic_ip,
		    filp->f_pos);

	cookie = filp->f_pos;

	for (;;) {
		if (!(len = Kfs_getDents(inode->u.generic_ip, 
					 (uval *)&cookie, buf, PAGE_SIZE))) {
			goto done;
		}
		
		KFS_DPRINTF(DebugMask::LINUX,
			    "(kfs_readdir) len=%llu\n", len);
		ptr = buf;
		while ((char *)ptr < (char *)buf + len) {
		    KFS_DPRINTF(DebugMask::LINUX,
				"(kfs_readdir) filldir name=%s, namlen=%d, d_off=%lld, pos=%lld, ino=%lld\n",
				ptr->d_name, strlen(ptr->d_name),
				ptr->d_off, filp->f_pos, ptr->d_ino);
			if (filldir(dirent, ptr->d_name, strlen(ptr->d_name),
				    filp->f_pos, ptr->d_ino, DT_UNKNOWN) < 0) {
			        KFS_DPRINTF(DebugMask::LINUX,
					    "filldir is filled up!\n");
				goto done;
			}
			if (!ptr->d_off) {
	          	        KFS_DPRINTF(DebugMask::LINUX,
					    "have reached the end!\n");
				filp->f_pos = cookie;
				goto done;
			}
			filp->f_pos = ptr->d_off;
			ptr = (struct dirent64 *)((char *)ptr + ptr->d_reclen);
		}
	}

   done:
	free_page((unsigned long)buf);
	mark_inode_dirty(inode);
	//	UPDATE_ATIME(inode);
	return 0;
}

/*
 * File creation. Allocate an inode, and we're done..
 */
static int
kfs_mknod(struct inode *dir, struct dentry *dentry, int mode, int dev)
{
	struct inode * inode = new_inode(dir->i_sb);
        int error = -ENOSPC;

        if (inode) {
		/* call KFS to register inode and get a FSFileKFS for it */
		switch (mode & S_IFMT) {
		case S_IFDIR:
			error = Kfs_createDir(dir->u.generic_ip, 
					      (char *)dentry->d_name.name,
					      dentry->d_name.len,  mode, 
					      &inode->u.generic_ip);
			break;
		case S_IFREG:
		default:
			error = Kfs_createFile(dir->u.generic_ip, 
					       (char *)dentry->d_name.name,
					       dentry->d_name.len,  mode,
					       &inode->u.generic_ip);
			break;
		}
		if (!error) {
			if (!inode->u.generic_ip) {
				panic("kfs_mknod() We have a problem 0x%p\n",
				      inode);
			}

			inode->i_ino = 0;

			kfs_read_inode(inode);
			kfs_read_inode(dir);

			insert_inode_hash(inode);
			d_instantiate(dentry, inode);

			mark_inode_dirty(inode);
			mark_inode_dirty(dir);
			dir->i_sb->s_dirt = 1;
		}
		else {
			kfs_set_inode_mode(inode);
			make_bad_inode(inode);
			iput(inode);
			KFS_DPRINTF(DebugMask::LINUX,
				    "(kfs_mknod) got error=%d\n", error);
			printk("(kfs_mknod) got error=%d\n", error);
		}
	}
        return error;
}

/* This is almost ther same as kfs_mknod() except the symlink() operation
 * needs the 'symname' argument...
*/
static int
kfs_symlink(struct inode *dir, struct dentry *dentry, const char * symname)
{
	struct inode * inode = new_inode(dir->i_sb);
        int error = -ENOSPC;

       error = Kfs_symlink(dir->u.generic_ip, (char *)dentry->d_name.name,
                           dentry->d_name.len, (char *)symname, &inode->u.generic_ip);

       if (!error) {
               inode->i_ino = 0;

               kfs_read_inode(inode);
               kfs_read_inode(dir);

               insert_inode_hash(inode);
               d_instantiate(dentry, inode);

               mark_inode_dirty(inode);
               mark_inode_dirty(dir);
               dir->i_sb->s_dirt = 1;
       }

        return error;
}

static int
kfs_readlink(struct dentry *dentry, char *buffer, int buflen)
{
	char kernel_buffer[buflen];
	int chars = Kfs_readlink(dentry->d_inode->u.generic_ip, kernel_buffer, buflen);
	kernel_buffer[chars] = 0;
	return vfs_readlink(dentry, buffer, buflen, kernel_buffer);
}

static int
kfs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	char kernel_buffer[256];
	int chars = Kfs_readlink(dentry->d_inode->u.generic_ip, kernel_buffer, 256);
	kernel_buffer[chars] = 0;
	return vfs_follow_link(nd, kernel_buffer);
}

static int 
kfs_mkdir(struct inode * dir, struct dentry * dentry, int mode)
{
        KFS_DPRINTF(DebugMask::LINUX,
		"(kfs_mkdir): dentry=%s\n", dentry->d_name.name);
        return kfs_mknod(dir, dentry, mode | S_IFDIR, 0);
}

static int
kfs_create(struct inode *dir, struct dentry *dentry, int mode)
{
        KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_create): dentry=%s\n", dentry->d_name.name);
        return kfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static int
kfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	int error;

	KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_unlink): dentry=%s fsFile=0x%p, inode=%ld IN\n",
		    dentry->d_name.name, inode->u.generic_ip, inode->i_ino);

	error = Kfs_unlink(dir->u.generic_ip, (char *)dentry->d_name.name,
			   dentry->d_name.len, inode->u.generic_ip);
	if (!error) {
		//kfs_read_inode(dir);    /* check for update on the dir */
		//kfs_read_inode(inode);  /* check for update on the inode */
		inode->i_nlink--;
		//mark_inode_dirty(dir);
		//mark_inode_dirty(inode);
		dir->i_sb->s_dirt = 1;
	}
	else {
         	KFS_DPRINTF(DebugMask::LINUX,
			    "(kfs_unlink) got error=%d\n", error);
	}

	KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_unlink): dentry=%s fsFile=0x%p, inode=%ld OUT\n",
		    dentry->d_name.name, inode->u.generic_ip, inode->i_ino);

	return error;
}

static int
kfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int error;

	KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_rmdir): dentry=%s\n", dentry->d_name.name);
	if (!(error = Kfs_rmdir(dir->u.generic_ip, (char *)dentry->d_name.name,
				dentry->d_name.len))) {
		//dentry->d_inode->i_nlink--;
		//dentry->d_inode->i_nlink--;

		kfs_read_inode(dentry->d_inode);
		kfs_read_inode(dir);    /* check for update on the dir */
		mark_inode_dirty(dentry->d_inode);
		mark_inode_dirty(dir);
		dir->i_sb->s_dirt = 1;
		KFS_DPRINTF(DebugMask::LINUX,
			    "(kfs_rmdir): inode->nlink=%d\n", dentry->d_inode->i_nlink);

	}
	return error;
}

static int 
kfs_rename(struct inode * old_dir, struct dentry * old_dentry,
	   struct inode * new_dir, struct dentry * new_dentry)
{
	int error;
	struct inode * old_inode = old_dentry->d_inode;
	struct inode * new_inode = new_dentry->d_inode;
	
	KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_rename): from=%s to=%s\n", old_dentry->d_name.name,
		    new_dentry->d_name.name);
	if (!(error = Kfs_rename(old_dir->u.generic_ip,
				 (char*) old_dentry->d_name.name,
				 old_dentry->d_name.len,
				 new_dir->u.generic_ip, 
				 (char*) new_dentry->d_name.name,
				 new_dentry->d_name.len,
				 old_inode->u.generic_ip))) {
		kfs_read_inode(old_dir);
		kfs_read_inode(new_dir);
		kfs_read_inode(old_inode);
		mark_inode_dirty(old_dir);
		mark_inode_dirty(new_dir);
		mark_inode_dirty(old_inode);
		if (new_inode) {
			kfs_read_inode(new_inode);
			mark_inode_dirty(new_inode);
		}
	}
	return error;

}

static void
kfs_write_inode(struct inode * inode, int wait)
{
//	printk("(kfs_write_inode) AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
	Kfs_write_inode_hook(inode->u.generic_ip);
}

static void 
kfs_delete_inode(struct inode *inode)
{
	int error;

	KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_delete_inode) Deleting fsfile 0x%p inode = %lu IN\n",
		    (void *)inode->u.generic_ip, inode->i_ino);

	if ((error = Kfs_deleteFile(inode->u.generic_ip)))
     	            KFS_DPRINTF(DebugMask::LINUX,
				"(lfs_delete_inode) OOOPS! Got ERROR=%d\n",
				error);
	inode->i_size = 0;
	clear_inode(inode);
	//inode->i_sb->s_dirt = 1;
	KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_delete_inode) Deleting fsfile=0x%p inode=%lu OUT\n",
		    (void *)inode->u.generic_ip, inode->i_ino);
}

static void
kfs_clear_inode(struct inode *inode)
{
        KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_clear_inode) Clearing fsfile = 0x%p inode = %ld IN\n",
		    (void *)inode->u.generic_ip, inode->i_ino);
	if (inode->u.generic_ip) {
		Kfs_clearInode(inode->u.generic_ip);
		inode->u.generic_ip = NULL;
	}
	KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_clear_inode) Clearing inode ino = %ld OUT\n",
		    inode->i_ino);
}

static void
kfs_write_super(struct super_block *sb)
{
//	KFS_DPRINTF(DebugMask::LINUX, "(kfs_write_super): \n");
	Kfs_writeSuper_hook();
	sb->s_dirt = 0;
}

static void kfs_kill_thread(void);

static void
kfs_put_super(struct super_block *sb)
{
	printk("killing super!\n");

	Kfs_put_super_hook();

	Kfs_writeSuper();
	Kfs_umount();

	printk("KFS memory usage:\n\tTotal memory=%ld\n\tNum allocations=%ld\n"
	       "\tNum deallocations=%ld\n"
	       "\tTotal reads=%ld (kfs=%ld page-nomap=%ld, page-map=%ld, b_dirty=%ld, b_clean=%ld)\n"
	       "\tTotal writes=%ld (kfs=%ld page-nomap=%ld, page-map=%ld, b=%ld)\n"
	       "\tTotal block allocs=%ld, block frees=%ld\n",
	       total_mem, num_alloc, num_dealloc,
	       total_reads, total_kfs_reads, reads_page_nomap, reads_page_map, reads_buffer_dirty, reads_buffer_clean,
	       total_writes, total_kfs_writes, writes_page_nomap, writes_page_map, writes_buffer,
	       block_cache_allocs, block_cache_frees);
}

static int 
kfs_notify_change(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	void * tok = inode->u.generic_ip;
	int error = 0;
	
	KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_notify_change) IN valid=0x%x\n", attr->ia_valid);
	
	if ((error = inode_change_ok(inode, attr)))
		return error;
	
	if (attr->ia_valid & ATTR_MODE)
		error = Kfs_fchmod(tok, attr->ia_mode);

	if (attr->ia_valid & ATTR_UID)
		error = Kfs_fchown(tok, attr->ia_uid, (gid_t)~0);

	if (attr->ia_valid & ATTR_GID)
		error = Kfs_fchown(tok, (uid_t)~0, attr->ia_gid);

	if (attr->ia_valid & ATTR_SIZE)
		if (attr->ia_size != inode->i_size) {
			error = Kfs_ftruncate(tok, attr->ia_size);
			inode->i_sb->s_dirt = 1;
		}

	if (attr->ia_valid & ATTR_ATIME)
		error = Kfs_utime(tok, NULL);
	
	if (!error)
		error = inode_setattr(inode, attr);

	kfs_read_inode(inode);
	mark_inode_dirty(inode);

	return error;
}

static int
kfs_readpage(struct file *file, struct page * page)
{
	int error;

	KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_readpage) page=0x%p, mapping=0x%p\n",
		    page, page->mapping);

	//printk("(kfs_readpage) page=0x%p, mapping=0x%p\n", page, page->mapping);

	//	KFS_DPRINTF(DebugMask::LINUX,
	//                  "(kfs_readpage) fsFile=0x%p, offset=%lld\n", 
	//		    file->f_dentry->d_inode->u.generic_ip, 
	//		    ((loff_t)page->index << PAGE_CACHE_SHIFT));

	total_kfs_reads++;
	
	if (page->buffers && buffer_uptodate(page->buffers)) {
		reads_page_map_buffer_uptodate++;
	}
	if (Page_Uptodate(page)) {
		reads_page_map_page_uptodate++;
	}

        error = Kfs_readBlockPhys(file->f_dentry->d_inode->u.generic_ip,
				  (uval) page_address(page),
				  ((loff_t)page->index << PAGE_CACHE_SHIFT));
	if (error) {
		printk("(kfs_readpage) Error! %d\n", error);
	}

	/* This must be a new *zero* page! */
	if (PageLocked(page) && !page->buffers) {
		SetPageUptodate(page);
		UnlockPage(page);
	}

	mark_inode_dirty(file->f_dentry->d_inode);

/*
  if (!error) {
  SetPageUptodate(page);
  }

  UnlockPage(page);
*/
	
        return error;
}

static int
kfs_prepare_write(struct file *file, struct page *page,
		  unsigned from, unsigned to)
{
	//KFS_DPRINTF(DebugMask::LINUX, "(kfs_prepare_write)\n");

//	SetPageUptodate(page);
//	SetPageDirty(page);

//	if (!page || !page_address(page)) {
//		err_printf("????????????????????\n");
//	}

	if (!Page_Uptodate(page)) {
		if (to < PAGE_SIZE) {
//			err_printf("1: to %u\n", to);
			memset(page_address(page) + to, 0, PAGE_SIZE - to);
		}
		
		if (from) {
//			err_printf("2: from %u\n", from);
			memset(page_address(page), 0, from);
		}
	}

	return 0;
}

static int
kfs_commit_write(struct file *file, struct page *page,
		 unsigned from, unsigned to)
{
        struct inode *inode = page->mapping->host;
        loff_t pos = ((loff_t)page->index << PAGE_CACHE_SHIFT);
        int error;

	//printk("(kfs_commit_write) PAGE=0x%p, tok=0x%p, from=%u, to=%u, pos=%lld\n",
	//page, (void *)inode->u.generic_ip, from, to, pos);

	total_kfs_writes++;

        if ((error = Kfs_writeBlockPhys(inode->u.generic_ip,
					(uval)page_address(page), to, pos))) {
		printk("(kfs_commit_write) ERROR! PAGE=0x%p, tok=0x%p, from=%u, to=%u, pos=%lld\n",
		       page, (void *)inode->u.generic_ip, from, to, pos);
		kfs_read_inode(inode);
                return error;
	}

	//	kfs_read_inode(inode);
	// possibly update the file size
	if (pos+to > inode->i_size) {
		inode->i_size = pos+to;
		inode->i_blocks = ALIGN_UP(pos+to, OS_BLOCK_SIZE) / OS_SECTOR_SIZE;
		mark_inode_dirty(inode);
	}

	inode->i_sb->s_dirt = 1;

        return 0;
}

static int
kfs_release_page(struct page *page, int gfp_mask)
{
//	err_printf("kfs_release_page() page=0x%p\n", page);
	if (page->buffers) {
		// see if this page has no users in the BlokCache,
		// and ask to unhash it
		return Kfs_releasePage(page->buffers->b_blocknr);
	}
	// sure, get rid of this page...
	return 1;
}

static int 
kfs_writepage(struct page *page)
{
        KFS_DPRINTF(DebugMask::LINUX,
		    "CAAAAAAALLLLLIIIIINNNNNGGGG WRITEPAGE: 0x%p\n", page);
	BUG();

        return 0;
}

static int 
kfs_sync_file (struct file *file, struct dentry *dentry, int datasync)
{
	struct inode *inode = file->f_dentry->d_inode;

	// FIXME FIXME FIXME: This is no good for K42!
	
	//first get the data out
	fsync_inode_data_buffers(inode);
	// now the meta-data
	Kfs_fsync(inode->u.generic_ip);
	return 0;
}

// this is a hack to see it read/write ops are working
static int
kfs_file_revalidate(struct dentry *dentry)
{
        KFS_DPRINTF(DebugMask::LINUX,
		    "(kfs_file_revalidate): %s\n", dentry->d_name.name);
//        truncate_inode_pages(dentry->d_inode->i_mapping, 0);
	kfs_read_inode(dentry->d_inode);
        return 0;
}

static struct super_operations kfs_sops = {
	read_inode:     kfs_read_inode,
	write_inode:    kfs_write_inode,
	//	put_inode:      kfs_put_inode,
	delete_inode:   kfs_delete_inode,
	put_super:      kfs_put_super,
	//	umount_begin:   kfs_put_super,
	write_super:    kfs_write_super,
	statfs:         kfs_statfs,
	clear_inode:    kfs_clear_inode,
};


static struct file_operations kfs_file_file_operations = {
	read:           generic_file_read,
	write:          generic_file_write,
	mmap:           generic_file_mmap,
	fsync:          kfs_sync_file,
};

static struct file_operations kfs_dir_file_operations = {
	read:           generic_read_dir,
        readdir:        kfs_readdir,
	fsync:          kfs_sync_file,
};

static struct inode_operations kfs_file_inode_operations = {
//	revalidate:     kfs_file_revalidate,  // this is a hack to see if read/write ops are working
	setattr:        kfs_notify_change,
};

static struct inode_operations kfs_dir_inode_operations = {
	create:         kfs_create,
	lookup:         kfs_lookup,
	link:           kfs_link,
	unlink:         kfs_unlink,
	mkdir:          kfs_mkdir,
	rmdir:          kfs_rmdir,
	mknod:          kfs_mknod,
	symlink:        kfs_symlink,
	rename:         kfs_rename,
//	revalidate:     kfs_file_revalidate,
	setattr:        kfs_notify_change,
};

static struct inode_operations kfs_symlink_inode_operations = {
	readlink:	kfs_readlink,
	follow_link:	kfs_follow_link,
};

static struct address_space_operations kfs_file_inode_aops = {
	readpage:       kfs_readpage,
        writepage:      kfs_writepage, //fail_writepage,
        prepare_write:  kfs_prepare_write,
        commit_write:   kfs_commit_write,
	sync_page:      block_sync_page,
//	commit_write:   generic_commit_write,
};

static struct super_block *kfsSuperBlock;
static struct inode *root_inode;

static void kfs_start_thread(void);

static struct super_block *
kfs_read_super(struct super_block * sb, void * data, int silent)
{
	struct inode * inode;
	struct dentry * root;
	void * fsFile;

	// need to set this as early as possible
	kfsSuperBlock = sb;

	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = KFS_SUPER_MAGIC;
	sb->s_op = &kfs_sops;

	inode = new_inode(sb);
	if (!inode)
		return NULL;

	inode->i_mode = S_IFDIR | 0755;
	kfs_set_inode_mode(inode);
	inode->i_ino = 0;  ///FIXME!!!

	root_inode = inode;

	fsFile = Kfs_init((char *)&(sb->s_dev), sb->s_flags & MS_RDONLY);
	if (!fsFile) {
		printk("(kfs_read_super) Oops, troube reading SuperBlock!\n");
		return NULL;
	}

	inode->u.generic_ip = fsFile;
	kfs_read_inode(inode);
	inode->i_mapping->a_ops->releasepage = kfs_release_page;

	insert_inode_hash(inode);

	root = d_alloc_root(inode);
	if (!root) {
		make_bad_inode(inode);
		iput(inode);
		return NULL;
	}

	sb->s_root = root;

	Kfs_read_super_hook();

	return sb;
}

static DECLARE_FSTYPE_DEV(kfs_fs_type, "kfs", kfs_read_super);

static int __init init_kfs_vfs(void)
{
#ifdef MODULE
//	__do_global_ctors_aux();
	//	__do_global_ctors();
#endif

	/* BlockCache slab cache */
	block_cache_entry_size = Kfs_sizeofbce();
	block_cachep = kmem_cache_create("kfs_block_cache", block_cache_entry_size,
					 0, SLAB_HWCACHE_ALIGN, NULL,
					 NULL);
	if (!block_cachep)
		panic("cannot create block_cache slab cache");
	return register_filesystem(&kfs_fs_type);
}
	
static void __exit exit_kfs_vfs(void)
{
	unregister_filesystem(&kfs_fs_type);
#ifdef MODULE
//	__do_global_dtors_aux();
	//	__do_global_dtors();
#endif
	kmem_cache_destroy(block_cachep);
}

module_init(init_kfs_vfs)
module_exit(exit_kfs_vfs)
MODULE_LICENSE("GPL");

int kfs_stop_thread;

struct kfs_thread_info_s {
	wait_queue_head_t wait_exit;
	int               stop;
} kfs_thread_info;

static int kfs_thread(void *arg)
{
	struct task_struct * tsk = current;
	int interval = HZ * 10;
	struct kfs_thread_info_s *kfs_thread_info = (struct kfs_thread_info_s *)arg;

	daemonize();
	reparent_to_init();
	sprintf(tsk->comm, "kfs_thread");

	printk("kfs_thread starting.  Commit interval %d seconds\n",
	       interval / HZ);

	/* And now, wait forever for commit wakeup events. */
	while (1) {
		interruptible_sleep_on_timeout(&kfs_thread_info->wait_exit, interval);

		KFS_DPRINTF(DebugMask::LINUX, "kfs_thread() activated...\n");

		if (kfs_thread_info->stop) {
			break;
		}
		
		// tell current superblock to sync this generation
		// this will automatically fork() the superblock, spawning
		// a new generation
		Kfs_writeSuper();
	}

	printk("kfs_thread stopping. Bye-Bye\n");
	wake_up(&kfs_thread_info->wait_exit);
	return 0;
}

static void kfs_start_thread(void)
{
	kfs_thread_info.stop = 0;
	init_waitqueue_head(&kfs_thread_info.wait_exit);

	kernel_thread(kfs_thread, (void *) &kfs_thread_info,
		      CLONE_VM | CLONE_FS | CLONE_FILES);
}

static void kfs_kill_thread(void)
{
	// do something to kill the thread...
	// this doesn't actually work. We should wait() until the
	// thread is actually dead. Perhaps signal it to wake it up.
	kfs_thread_info.stop = 1;
	wake_up(&kfs_thread_info.wait_exit);
	sleep_on(&kfs_thread_info.wait_exit);
	printk("kfs_kill_thread() woke-up\n");
}

/***************************************************************/

/*
 * These next functions are methods which serve as an "interface"
 * for KFS' C++ code to communicate with Linux's native methods.
 */

#include <linux/blkdev.h>

/********************* MEMORY ALLOCATION ********************************/

/*
 *  CAUTION: Allocation only works here due to the fact that KFS only
 *           allocates at most 4096. If we ever should pass this value,
 *           these allocation procedures should change to vmalloc()/vfree()
 */
void *
allocGlobal(uval size)
{
	void *ptr = NULL;

	if (!size) {
		printk("O QUE?\n");
		BUG();
	}

	if (size > 131072) {
	    printk("Too big! size=%lu\n", size);
	    BUG();
	}

	total_mem+=size;
	num_alloc++;
	//KFS_DPRINTF(DebugMask::LINUX,
	//            "allocGlobal: Trying to alloc %ld, total=%ld",
	//            size, total_mem);

	/*
	if (size == PAGE_SIZE) {
		ptr = (void *)__get_free_page(GFP_KERNEL);
	} else if (size ==  block_cache_entry_size) {
		block_cache_allocs++;
		ptr = kmem_cache_alloc(block_cachep, SLAB_KERNEL);
	} else {
		ptr = kmalloc(size, GFP_KERNEL);
		while (!ptr) {
			err_printf("KFS failed memory allocation of size=%ld, trying again\n", size);
			schedule();
			ptr = kmalloc(size, GFP_KERNEL);
		}
	}
	*/

	switch (size) {
	case PAGE_SIZE:
		ptr = (void *)__get_free_page(GFP_KERNEL);
		break;
	default:
		ptr = kmalloc(size, GFP_KERNEL);
		while (!ptr) {
			err_printf("KFS failed memory allocation of size=%ld, trying again\n", size);
			schedule();
			ptr = kmalloc(size, GFP_KERNEL);
		}
	}

	if (!ptr) {
		printk(KERN_EMERG "KFS failed memory allocation of size=%ld\n", size);
		BUG();
	}
	//KFS_DPRINTF(DebugMask::LINUX, " ... ptr  = 0x%p\n", ptr);
	return ptr;
}

void
freeGlobal(void *ptr)
{ 
	err_printf("freeGlobal: Trying to free %p\n", ptr);

	num_dealloc++;

	//BUG();
	/* dangerous! what if we have a `page'?? */
	/* try to make sure that __builtin* are the only users */
	kfree(ptr);
//	vfree(ptr);
}

void
freeGlobalWithSize(void *ptr, int size)
{ 
	total_mem-=size;
	num_dealloc++;
	//KFS_DPRINTF(DebugMask::LINUX,
	//            "freeGlobal: Trying to free 0x%p, size=%d, total=%ld\n",
	//            ptr, size, total_mem);

	if (!size) {
		printk("freeGlobalWithSize O QUE?\n");
		BUG();
	}

/*	if (size == PAGE_SIZE) {
		free_page((unsigned long)ptr);
	} else if (size == block_cache_entry_size) {
		block_cache_frees++;
		kmem_cache_free(block_cachep, (ptr));
	} else {
			kfree(ptr);
			} */
	switch (size) {
	case PAGE_SIZE:
		free_page((unsigned long)ptr);
		break;
	default:
		kfree(ptr);
	}
}

void *
LinuxAllocPage(void)
{
	return alloc_page(GFP_KERNEL);
}

/********************* BLOCKS AND I/O ********************************/

/* BlockCacheEntry */
void *
LinuxGetBlock(uval32 b)
{
	return (void *) getblk(kfsSuperBlock->s_dev, b, PAGE_SIZE);
}

void *
LinuxGetPage(uval32 b)
{
	struct page *page = find_or_create_page(root_inode->i_mapping, b, GFP_KERNEL);
//	put_page(page);
	UnlockPage(page);
	return (void *)page;
}

void
LinuxFreePage(void *page)
{
	//__free_page((struct page *)page);
	put_page(page);
}

void
LinuxFreeBlock(void *bh)
{
	brelse((struct buffer_head *)bh);
}

void
LinuxCleanBlock(void *b)
{
	struct buffer_head *bh = (struct buffer_head *)b;
	lock_buffer(bh);
	mark_buffer_clean(bh);
	mark_buffer_uptodate(bh, 1);
	SetPageUptodate(bh->b_page);
	unlock_buffer(bh);
}

void
LinuxCleanPage(void *page, uval32 b)
{
	struct buffer_head *bh = ((struct page *)page)->buffers;

	if (bh) {
		lock_buffer(bh);
		mark_buffer_clean(bh);
		mark_buffer_uptodate(bh, 1);
		Kfs_LinuxCleanPage_hook(bh);
		unlock_buffer(bh);
	}
	SetPageUptodate((struct page *)page);
}

char *
LinuxGetBlockData(void *bh)
{
	return ((struct buffer_head *)bh)->b_data;
}

char *
LinuxGetPageData(void *page)
{
	return page_address((struct page *)page);
}

void
LinuxDirtyBlock(void *b)
{
	struct buffer_head *bh = (struct buffer_head *)b;
	lock_buffer(bh);
	set_buffer_async_io(bh);
	mark_buffer_uptodate(bh, 1);
	mark_buffer_dirty(bh);
	unlock_buffer(bh);

	writes_buffer++;
}

void
LinuxDirtyPage(void **p, uval32 b, void *sb)
{
	struct page *page = *p;

	if (!page->buffers) {
		create_empty_buffers(page, kfsSuperBlock->s_dev, PAGE_SIZE);
		clear_bit(BH_New, &page->buffers->b_state);
		page->buffers->b_blocknr = b;
		page->buffers->b_state |= (1UL << BH_Mapped);
	}

	KFS_DPRINTF(DebugMask::LINUX,
		    "(LinuxDirtyPage) data=0x%p, page=0x%p, mapping=0x%p, bh=0x%p\n",
		    page_address(page), page, page->mapping, page->buffers);

	lock_buffer(page->buffers);

	Kfs_LinuxDirtyPage_hook(page, sb);

	mark_buffer_uptodate(page->buffers, 1);
	mark_buffer_dirty(page->buffers);

	unlock_buffer(page->buffers);

	SetPageUptodate((struct page *)page);

	total_writes++;
	writes_buffer++;
}

void
LinuxReadBlock(void *b)
{
	struct buffer_head *bh = (struct buffer_head *)b;

	if (!buffer_uptodate(bh)) {
		ll_rw_block(READ, 1, &bh);
		wait_on_buffer(bh);
		//bh2 = bread(bh->b_dev, bh->b_blocknr, bh->b_size);
		//brelse(bh2);
		reads_buffer_dirty++;
	}
	else {
		reads_buffer_clean++;
	}
	total_reads++;
}

static void
kfs_end_buffer_io_async(struct buffer_head * bh, int uptodate);

void
LinuxReadPage(void *p, uval32 b)
{
	struct page *page = (struct page *)p;

//	lock_page(page);
	
	if (!page->buffers) {
		create_empty_buffers(page, kfsSuperBlock->s_dev, PAGE_SIZE);

		page->buffers->b_blocknr = b;
		mark_buffer_uptodate(page->buffers, 0);
		clear_bit(BH_New, &page->buffers->b_state);
		page->buffers->b_state |= (1UL << BH_Mapped);
	}

	KFS_DPRINTF(DebugMask::LINUX,
		    "(LinuxReadPage) data=0x%p, page=0x%p, mapping=0x%p, bh=0x%p\n",
		    page_address(page), page, page->mapping, page->buffers);

	/* If not uptodate, then we have to actually read the page */
	if (!buffer_uptodate(page->buffers)) {
		ll_rw_block(READ, 1, &(page->buffers));
		wait_on_buffer(page->buffers);
		reads_buffer_dirty++;
	}
	else {
		reads_buffer_clean++;
	}
	total_reads++;
	SetPageUptodate(page);
//	unlock_buffer(page->buffers);
//	UnlockPage(page);
}

SysStatus 
LinuxBlockReadOLD(void *data, kdev_t dev, int block, int size)
{
	struct buffer_head *bh;
//	struct page *page, *page_data;

	total_reads++;

	if (!(bh = bread(dev, block, size))) {
		printk("(LinuxBlockRead) EEEEEEEEEEERRRRRRRROOOOOOOOOOOORRRR!\n");
		BUG(); // currently put a BUG() for debugging
		return -1; // how to return an ERROR back to KFS?
	}
	if (data == bh->b_data) {
   	        KFS_DPRINTF(DebugMask::LINUX,
			    "LinuxBlockRead() SAME PLACE!\n");
	}
	else {
	        KFS_DPRINTF(DebugMask::LINUX,
			    "LinuxBlockRead() different place!\n");
		memcpy(data, bh->b_data, size);
	}

	brelse(bh);
	return 0;
}

int 
kfs_get_block(struct inode *inode, long iblock, struct buffer_head *bh_result,int create)
{
	err_printf("kfs_get_block called! :-(\n");
	BUG();
	return -1;
}

static void
kfs_end_buffer_io_async(struct buffer_head * bh, int uptodate)
{
	struct page *page;

	/* This is a temporary buffer used for page I/O. */
	page = bh->b_page;

//	printk("(kfs_end_buffer_io_async) page=0x%p, mapping=0x%p\n", page, page->mapping);

	//mark_buffer_uptodate(bh, uptodate);
	mark_buffer_uptodate(bh, 1);
	mark_buffer_async(bh, 0);
	unlock_buffer(bh);
	SetPageUptodate(page);
	UnlockPage(page);
}

SysStatus 
LinuxBlockRead(void *data, kdev_t dev, int block, int size)
{
	struct page *page;

	page = virt_to_page(data);

/*
	KFS_DPRINTF(DebugMask::LINUX,
		    "(LinuxBlockRead) page_data=0x%p\n", page_data);
	KFS_DPRINTF(DebugMask::LINUX,
		    "(LinuxBlockRead) mapping=0x%p\n", page_data->mapping);

	printk("(LinuxBlockRead) date=0x%p, page=0x%p, mapping=0x%p\n",
	       data, page, page->mapping);
*/
	total_reads++;

	if (!page->mapping) {
		struct buffer_head *bh;

		reads_page_nomap++;
		KFS_DPRINTF(DebugMask::LINUX,
			    "(LinuxBlockRead) no mapping, page=0x%p, "
			    "data=0x%p\n", page, data);
		// Page *not* from the page cache. Just read from the
		// buffer cache and copy the contents. Hopefully,
		// someday, KFS will cease to contain pages which read
		// through generic block functions which are not from
		// any cache
		if (!(bh = bread(dev, block, size))) {
			printk("(LinuxBlockRead) EEEEEEEEEEERRRRRRRROOOOOOOOOOOORRRR!\n");
			BUG(); // currently put a BUG() for debugging
			return -1; // how to return an ERROR back to KFS?
		}
		if (data != bh->b_data) {
			memcpy(data, bh->b_data, size);
		}
		brelse(bh);
	}
	else {
		// Page from the page cache. Great, just read it.
		int locked = 0;
		KFS_DPRINTF(DebugMask::LINUX,
			    "(LinuxBlockRead) mapping=0x%p, page=0x%p, "
			    "data=0x%p\n", page->mapping, page, data);

		reads_page_map++;

		if (!PageLocked(page)) {
//			KFS_DPRINTF(DebugMask::LINUX,
//		                    "(LinuxBlockRead) not locked!!!!\n");
//			printk("(LinuxBlockRead) not locked!!!! page=0x%p\n", page);
			lock_page(page);
			locked = 1;
		}

		if (!page->buffers) {
			create_empty_buffers(page, dev, size);

			reads_page_map_no_bh++;
//			lock_buffer(page->buffers);
			page->buffers->b_blocknr = block;
			mark_buffer_uptodate(page->buffers, 0);
			clear_bit(BH_New, &page->buffers->b_state);
			page->buffers->b_state |= (1UL << BH_Mapped);
//			unlock_buffer(page->buffers);
		}
		lock_buffer(page->buffers);
		/* If not uptodate, then we have to actually read the page */
		if (!buffer_uptodate(page->buffers)) {
			//block_read_full_page(page, kfs_get_block);
			set_buffer_async_io(page->buffers);
			page->buffers->b_end_io = kfs_end_buffer_io_async;
			submit_bh(READ, page->buffers);
		}
		else {
			SetPageUptodate(page);
			unlock_buffer(page->buffers);
			UnlockPage(page);
			locked = 0;
		}
		
		// This is a page from KFS. We need to wait for the
		// read to actually finish before returning
		if (locked) {
			wait_on_page(page);
		}

		//ll_rw_block(READ, 1, &page->buffers);
		/* Must wait on buffer to get actually read. This will
		   also wait until b_end_io is executed and,
		   therefore, the page will be unlocked */
		/*
		  wait_on_buffer(page->buffers);
		  SetPageUptodate(page);
		  if (PageLocked(page)) {
		  UnlockPage(page);
		  }
		*/
	}

	return 0;
}

SysStatus
LinuxBlockWriteOLD(void *data, kdev_t dev, int block, int size)
{
	// OLD VERSION WITH SYNC IO. DO NOT DELETE FOR TESTS
	struct buffer_head *bh = getblk(dev, block, size);

	total_writes++;

//	KFS_DPRINTF(DebugMask::LINUX,
//  	            "(LinuxVFS::LinuxBlockWrite) dev=%d, block=%d, size=%d\n",
//		    dev, block, size);

	lock_buffer(bh);
	if (data == bh->b_data) {
	        KFS_DPRINTF(DebugMask::LINUX,
			    "LinuxBlockWrite() SAME PLACE!\n");
	}
	else {
	        KFS_DPRINTF(DebugMask::LINUX,
			    "LinuxBlockWrite() different place!\n");
		memcpy(bh->b_data, data, size);
	}
	//memcpy(bh->b_data, data, size);
	mark_buffer_uptodate(bh, 1);
	mark_buffer_dirty(bh);
	unlock_buffer(bh);

//	ll_rw_block(WRITE, 1, &bh);  // UNCOMMENT THIS FOR SYNC I/O
//	wait_on_buffer(bh);          // UNCOMMENT THIS FOR SYNC I/O

	brelse(bh);

	return 0;
}

SysStatus
LinuxBlockWrite(void *data, kdev_t dev, int block, int size)
{
	struct buffer_head *bh;
	struct page *page;

	page = virt_to_page(data);

	total_writes++;

	KFS_DPRINTF(DebugMask::LINUX,
		    "(LinuxBlockWrite) data=0x%p, page=0x%p, mapping=0x%p\n",
		    data, page, page->mapping);

	if (!page->mapping) {
		// Page *not* from the page cache. Just read from the
		// buffer cache and copy the contents. Hopefully,
		// someday, KFS will cease to contain pages which read
		// through generic block functions which are not from
		// any cache
		
		writes_page_nomap++;
		bh = getblk(dev, block, size);

		KFS_DPRINTF(DebugMask::LINUX,
			    "(LinuxBlockWrite) no mapping, page=0x%p, "
			    "data=0x%p\n", page, data);

		lock_buffer(bh);
		if (data != bh->b_data) {
			memcpy(bh->b_data, data, size);
		}
		//set_buffer_async_io(bh);
		mark_buffer_uptodate(bh, 1);
		mark_buffer_dirty(bh);
		unlock_buffer(bh);
		
		brelse(bh);
	}
	else {
		// Page from the page cache. Great, just issue a write on its behalf.
//		int locked = 0;

	        KFS_DPRINTF(DebugMask::LINUX,
			    "(LinuxBlockWrite) mapping=0x%p, page=0x%p, "
			    "data=0x%p\n", page->mapping, page, data);
		
		writes_page_map++;

//		if (!PageLocked(page)) {
//			lock_page(page);
//			locked = 1;
//	        }
		if (!page->buffers) {
			create_empty_buffers(page, dev, size);

//			lock_buffer(page->buffers);
			clear_bit(BH_New, &page->buffers->b_state);
			page->buffers->b_blocknr = block;
			page->buffers->b_state |= (1UL << BH_Mapped);
//			unlock_buffer(page->buffers);
		}

		lock_buffer(page->buffers);
		//set_buffer_async_io(page->buffers);

		mark_buffer_uptodate(page->buffers, 1);
		mark_buffer_dirty(page->buffers);
		SetPageUptodate(page);
		// CHANGED ON 2.4.22!		if (page->mapping->host != page->buffers->b_inode) {
		buffer_insert_inode_data_queue(page->buffers, page->mapping->host);
		// CHANGED ON 2.4.22! }

		unlock_buffer(page->buffers);

		//		if (locked) {
		//			unlock_page(page);
		//		}

		//SetPageUptodate(page);
	}

	return 0;
}

inline int
LinuxGetHardsectSize(kdev_t dev)
{
	return get_hardsect_size(dev);
}

/********************* LOCKS AND SEMAPHORES ********************************/

/*
 *   Lock implementation!
 */

#include <asm/semaphore.h>

inline size_t
sizeOfLinuxSemaphore(void)
{
	return sizeof(struct semaphore);
}

inline void
newLinuxSemaphore(struct semaphore *sem)
{
	init_MUTEX(sem);
//	KFS_DPRINTF(DebugMask::LINUX, "initializing new lock=0x%p\n", sem);
}

inline void
acquireLinuxSemaphore(struct semaphore *sem)
{
//	KFS_DPRINTF(DebugMask::LINUX, "locking 0x%p\n", sem);
	down(sem);
}

inline void
releaseLinuxSemaphore(struct semaphore *sem)
{
	if ((void *)sem == (void *)0x5a5a5a5a)
		printk("unlocking 0x%p\n", sem);
	up(sem);
}

// returns 1 if lock gotten, 0 if not
inline int
tryAcquireLinuxSemaphore(struct semaphore *sem) 
{
//	KFS_DPRINTF(DebugMask::LINUX, "trying lock 0x%p\n", sem);
	return (!down_trylock(sem));
}

inline int
isLockedLinuxSemaphore(struct semaphore *sem)
{
//	KFS_DPRINTF(DebugMask::LINUX, "is locked? 0x%p\n", sem);
	return (atomic_read(&sem->count) != 1);
}


/*
 *   Read-Write Lock implementation!
 */

#include <linux/rwsem.h>

inline size_t
sizeOfLinuxRWSemaphore(void)
{
	return sizeof(struct rw_semaphore);
}

inline void
newLinuxRWSemaphore(struct rw_semaphore *sem)
{
	init_rwsem(sem);
//	KFS_DPRINTF(DebugMask::LINUX, "initializing new lock=0x%p\n", sem);
}

inline void
acquireReadLinuxSemaphore(struct rw_semaphore *sem)
{
//	KFS_DPRINTF(DebugMask::LINUX, "locking 0x%p\n", sem);
	down_read(sem);
}

inline void
acquireWriteLinuxSemaphore(struct rw_semaphore *sem)
{
//	KFS_DPRINTF(DebugMask::LINUX, "locking 0x%p\n", sem);
	down_write(sem);
}

inline void
releaseReadLinuxSemaphore(struct rw_semaphore *sem)
{
	if ((void *)sem == (void *)0x5a5a5a5a)
		printk("unlocking 0x%p\n", sem);
	up_read(sem);
}

inline void
releaseWriteLinuxSemaphore(struct rw_semaphore *sem)
{
	if ((void *)sem == (void *)0x5a5a5a5a)
		printk("unlocking 0x%p\n", sem);
	up_write(sem);
}
