/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <io/FileLinux.H>
#include <io/FileLinuxVirtFile.H>
#include <io/VirtFile.H>
#include <meta/MetaVirtFile.H>
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>
#include <usr/ProgExec.H>
#include <sys/systemAccess.H>

class VirtFileImp: public VirtFile {
private:
    ObjectHandle oh;
    uval read_counter, write_counter;
    uval read_size;
    char data;  // value changed by write
public:
    DEFINE_GLOBAL_NEW(VirtFileImp);
    VirtFileImp(uval rsize) {
	read_counter = 0;
	write_counter = 0;
	read_size = rsize;
	data = '.';
	CObjRootSingleRep::Create(this);
	XHandle xHandle;
	// initialize xhandle, use the same for everyone
	xHandle = MetaVirtFile::createXHandle(getRef(), GOBJ(TheProcessRef),
					      MetaVirtFile::none,
					      MetaVirtFile::none);

	oh.initWithMyPID(xHandle);
    }
    DEFINE_ACCESS_SERVER(VirtFile, MetaObj::none, MetaObj::none);

    ObjectHandle getOH() {return oh;}

    virtual SysStatus _getMaxReadSize(uval &max, uval token=0) {
	max = read_size;
	return 0;
    }

    // synchronous read interface where offset is passed as argument
    virtual SysStatusUval _read (char *buf, uval length, uval offset,
				 __XHANDLE xhandle) {
	// no synchronization necessary (file system library provides sync)
	char msg[64];
	read_counter++;
	sprintf(msg, "data is %c, read_counter is 0x%lx, write_counter "
		"is 0x%lx\n", data, read_counter, write_counter);

	uval msgsz = strlen(msg);
	if (length > read_size) {
	    length = read_size;
	}
	if (msgsz > length) {
	    msgsz = length;
	}
	if (offset < length) {
	    char *buf_ptr = buf;
	    uval count = length;
	    // output is a msg followed by a sequence of '*' and then a '#'
	    if (offset < msgsz) {
		uval tmp = msgsz - offset;
		memcpy(buf_ptr, &msg[offset], tmp);
		buf_ptr += tmp;
		count -= tmp;
	    }
	    if (count > 1) {
		memset(buf_ptr, '*', count - 1);
		buf_ptr += count - 1;
	    }
	    if (count > 0) {
		memset(buf_ptr, '#', 1);
	    }
	    return _SRETUVAL(length-offset);
	} else {
	    return _SERROR(2473, FileLinux::EndOfFile, 0);
	}
    }

    // synchronous read interface where whole file is passed back
    virtual SysStatusUval _read (char *buf, uval buflength, __XHANDLE xhandle) {
	// no synchronization necessary (file system library provides sync)
	char msg[64];
	read_counter++;
	sprintf(msg, "data is %c read_counter is 0x%lx, write_counter is "
		"0x%lx\n", data, read_counter, write_counter);

	if (buflength < read_size) {
	    return _SERROR(2162, 0, ENOSPC);
	}

	uval length = read_size;
	uval msgsz = strlen(msg);
	if (msgsz > buflength) {
	    msgsz = buflength;
	}
	char *buf_ptr = buf;
	uval count = length;
	// output is a msg followed by a sequence of '*' and then a '#'
	memcpy(buf_ptr, msg, msgsz);
	buf_ptr += msgsz;
	count -= msgsz;

	if (count > 1) {
	    memset(buf_ptr, '*', count - 1);
	    buf_ptr += count - 1;
	}
	if (count > 0) {
	    memset(buf_ptr, '#', 1);
	}
	return _SRETUVAL(length);
    }

    virtual SysStatusUval _write(const char *buf, uval length,
				 __XHANDLE xhandle) {
	// no synchronization necessary (file system library provides sync)
	write_counter++;
	if (sizeof(char) <= length) {
	    data = buf[0];
	}
	return length;
    }
    virtual SysStatus _open(uval oflags) {
	// no synchronization necessary (file system library provides sync)
	if (oflags & O_TRUNC) {
	    // restore initial values
	    read_counter = write_counter = 0;
	    data = '.';
	}
	return 0;
    }
    virtual SysStatus _close() {
	// nothing to do on close
	return 0;
    }
};

static void
usage(char *prog)
{
    printf("Usage: %s filename max_read_size\n"
	       "\ttests FileSystemVirtFile\n"
	       "\tfilename is the virtual file to be created\n"
	       "\tmax_read_size is the maximum number of bytes to be returned "
	       "\t\tby the read op\n", prog);
}

static void
startupSecondaryProcessors(char *prog)
{
    SysStatus rc;
    VPNum n, vp;
    n = DREFGOBJ(TheProcessRef)->ppCount();

    if ( n <= 1) {
        err_printf("%s - number of processors %ld\n", prog, n);
        return ;
    }

    err_printf("%s - starting %ld secondary processors\n", prog, n-1);
    for (vp = 1; vp < n; vp++) {
        rc = ProgExec::CreateVP(vp);
        passert(_SUCCESS(rc),
		err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
        err_printf("%s - vp %ld created\n", prog, vp);
    }
}

int
main(int argc, char *argv[])
{
    NativeProcess();

    if (argc != 3) {
	usage(argv[0]);
	return 1;
    }

    char *file = argv[1];

    char *endptr;
    uval max_read_size;
    SysStatus rc = baseStrtol(argv[2], &endptr, 10, max_read_size);
    if (_FAILURE(rc)) {
	printf("%s: length %s provided for file %s is invalid\n",
		   argv[0], argv[2], file);
	usage(argv[0]);
	return (1);
    }

    VirtFileImp *vf = new VirtFileImp(max_read_size);

    rc = FileLinuxVirtFile::CreateFile(file, (mode_t) 0777,
				       vf->getOH());
    if (_FAILURE(rc)) {
	if (_SGENCD(rc) == EEXIST) {
	    printf("%s: File %s already exists\n", argv[0], file);
	} else if (_SGENCD(rc) == ENOTDIR) {
	    printf("%s: a component used as a directory in pathname %s "
		       "is not, in fact, a directory\n", argv[0], file);
	} else if (_SGENCD(rc) == ENOENT) {
	    printf("%s: a directory component in %s does not exist\n",
		       argv[0], file);
	} else {
	    printf("%s: FileLinuxVirtFile::CreateFile for %s failed: "
		       "(%ld, %ld, %ld)\n", argv[0], file, _SERRCD(rc),
		       _SCLSCD(rc), _SGENCD(rc));
	}
	return 1;
    }

    printf("%s:\tThis program will block itself and run indefinetly\n"
	       "\t\tYou'll have to kill it manually...\n", argv[0]);
    // deactivate and block this child's thread forever
    Scheduler::DeactivateSelf();
    while (1) {
	Scheduler::Block();
	cprintf("AFter Scheduler::Block()\n");
    }
    return 0;
}
