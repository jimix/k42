/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br)
 *
 * $Id: FileDisk.C,v 1.12 2004/09/15 20:49:45 dilma Exp $
 *****************************************************************************/

#define _LARGEFILE64_SOURCE

#include "FileDisk.H"
#include "PSOBase.H"  // for OS_BLOCK_SIZE

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>


/*
 * FileDisk()
 *
 *   Initialize the disk size and open the file that will act as a disk.
 */
FileDisk::FileDisk(int filefd, uval nBlocks, uval bSize)
    : numBlocks(nBlocks), blockSize(bSize)
{
    fd = filefd;
    char buf[blockSize];

    // lock it
    fileLock.l_type = F_WRLCK;
    fcntl(fd, F_SETLKW, &fileLock);

    // write out a single block to the end of the file to set the size
#if defined (KFS_TOOLS) && defined(PLATFORM_OS_Darwin)
    if (lseek(fd, (numBlocks - 1) * blockSize, SEEK_SET) == -1) {
#else
    if (lseek64(fd, (numBlocks - 1) * blockSize, SEEK_SET) == -1) {
#endif
	printf("Aborting on error: %s\n", strerror(errno));
    }
    memset(buf, 0, blockSize);
    write(fd, buf, blockSize);
}


/*
 *  valid_offset()
 *
 *    From the e2fsprogs code. Determines if a particular offset is
 *    valid in a file. Used to dicover the size of files/devices.
 */
static int
valid_offset (int fd, uval64 offset)
{
    char ch;

    if (lseek (fd, offset, SEEK_SET) < 0)
	return 0;
    if (read (fd, &ch, 1) < 1)
	return 0;
    return 1;
}

/*
 * FileDisk()
 *
 *    Initialize a disk with a pre-existing image.
 */
FileDisk::FileDisk(int filefd)
{
    fd = filefd;

    // lock it
    fileLock.l_type = F_WRLCK;
    fcntl(fd, F_SETLKW, &fileLock);

    struct stat buf;
    // Discover number of blocks and blockSize
    fstat(fd, &buf);

    blockSize = OS_BLOCK_SIZE;
    if (!buf.st_size) {
	uval64 low, high, mid;
	// Oops, have to do it the hard way!
	low = 0;
        for (high = 1024; valid_offset (fd, high); high *= 2)
	    low = high;
        while (low < high - 1) {
	    mid = (low + high) / 2;

	    if (valid_offset (fd, mid))
		low = mid;
	    else
		high = mid;
        }
        valid_offset (fd, 0);
	buf.st_size = low + 1;
    }
    numBlocks = buf.st_size/blockSize;
}

/*
 * ~FileDisk()
 *
 *   Unlock the associated file and close it.
 */
FileDisk::~FileDisk()
{
    fileLock.l_type = F_UNLCK;
    fcntl(fd, F_SETLKW, &fileLock);

    close(fd);
}

/*
 * ReadCapacity()
 *
 *   Return the disk capacity characteristics.
 */
SysStatus
FileDisk::readCapacity(uval &nBlocks, uval &bSize)
{
    nBlocks = numBlocks;
    bSize = blockSize;

    return 0;
}

/*
 * AReadBlock()
 *
 *   No asynchronous reads... just call the synchronous read.
 */
SysStatus
FileDisk::aReadBlock(uval blkno, char *buf, PSOBase::AsyncOpInfo *cont)
{
    return readBlock(blkno, buf);
}

/*
 * ReadBlock()
 *
 *   Read in the requested block from the file.
 */
SysStatus
FileDisk::readBlock(uval blkno, char *buf)
{
    sval rc;

    if (blkno >= numBlocks) {
	tassertMsg(0, "blkno=%lu, numBlocks=%lu", blkno, numBlocks);
        return -1;
    }

    rc = lseek(fd, blkno * blockSize, SEEK_SET);
    if (rc < 0) { return rc; }

    rc = read(fd, buf, blockSize);
    if (rc < 0) { return rc; }

    return 0;
}

/*
 * AWriteBlock()
 *
 *   No asynchronous writes... just call the synchronous write.
 */
SysStatus
FileDisk::aWriteBlock(uval blkno, char *buf, PSOBase::AsyncOpInfo *cont)
{
    return writeBlock(blkno, buf);
}

/*
 * WriteBlock()
 *
 *   Write the given block to the file.
 */
SysStatus
FileDisk::writeBlock(uval blkno, char *buf)
{
    sval rc;

    if (blkno >= numBlocks) {
	tassertMsg(0, "?");
        return -1;
    }

    rc = lseek(fd, blkno * blockSize, SEEK_SET);
    if (rc < 0) { return rc; }

    rc = write(fd, buf, blockSize);

    return rc;
}

