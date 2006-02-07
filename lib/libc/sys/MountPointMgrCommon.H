#ifndef __MOUNT_POINT_MGR_COMMON_H_
#define __MOUNT_POINT_MGR_COMMON_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MountPointMgrCommon.H,v 1.21 2005/01/13 22:33:44 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implements functionality common to server and
 * client mount point objects.  Have chosen to implement this as a
 * straight C++ object invoked by client and server objects, rather
 * than as a common superclass since gives more flexibility in future
 * rearrangement of code.  For now, all locking in client or server
 * class that invokes this class.
 * **************************************************************************/
#include <misc/ListSimple.H>
// For now dirent == direntk42, will change to dirent64 in ELF glibc
#ifndef direntk42
#define direntk42 dirent64
#endif //#ifndef direntk42

class MountPointMgrCommon {
    static const uval MAX_MP_DESC_LEN = 128; /* max size of string representing
					      * a mount point description */
    class History;
    // The following is used by method printMtab to specify which type
    // of information is desired. Visible means that the mount point or
    // bind is not currently covered by another mount point up in the tree
    // representing the name space.
    typedef enum {VISIBLE_MP, VISIBLE_BND, INVISIBLE} VisibilityType;
protected:

    // state describing element in tree, may or may not be a mount point
    class MountComponent {
	class MountPointInfo {
	    ObjectHandle oh;	  // name space object handle
	    PathNameDynamic<AllocGlobal> *relPath; // pathname in server
	    uval relLen;		  // length of path
	    char *desc;                   // string describing the MP
	    uval descLen;                 // length of string desc
	    uval isBind;                  // indicates this is from a Bind op
	public:
	    DEFINE_GLOBALPADDED_NEW(MountPointInfo);
	    MountPointInfo(ObjectHandle poh, const PathName *pth,
			   uval pthLen, const char *desc, uval descLen,
			   uval isBind);
	    ~MountPointInfo();

	    PathNameDynamic<AllocGlobal> *getRelPath() {
		return relPath;
	    }
	    uval getRelLen() { return relLen; }
	    ObjectHandle *getOH() { return &oh; }
	    char *getDesc() { return desc; }
	    uval getDescLen() { return descLen; }
	    uval getIsBind() { return isBind; }
	};

	// state describing a mount point
	class SymbolicLinkInfo {
	    char *data;
	    uval dataLen;
	public:
	    DEFINE_GLOBALPADDED_NEW(SymbolicLinkInfo);
	    SymbolicLinkInfo(const char *str, uval len);
	    ~SymbolicLinkInfo();
	    char *getData() { return data; }
	};

	ListSimple<MountComponent *, AllocGlobal> children;
	union {
	    MountPointInfo *mountPointInfo;
	    SymbolicLinkInfo *symbolicLinkInfo;
	};
	enum {
	    MOUNT    = 1,
	    COVMOUNT = 2,		// is a coverable mount point
	    SYMLINK  = 3,
	    EMPTY    = 4		// empty intermediate node
	} compType;

    public:
	DEFINE_GLOBALPADDED_NEW(MountComponent);
	char *comp;			// string name of this component
	uval8 compLen;			// length of component (with null term)

	void addChild(MountComponent *child) {
	    children.add(child);
	}

	uval isCoverable() {return (compType == COVMOUNT);}
	uval isMountPoint() {
	    return (compType == COVMOUNT) || (compType == MOUNT);
	}
	uval isSymLink()   {return (compType == SYMLINK);}
	uval isEmpty()     {return (compType == EMPTY);}

	PathNameDynamic<AllocGlobal> *getMTRelPath() {
	    tassertMsg(isMountPoint(), "woops\n");
	    return mountPointInfo->getRelPath();
	}
	uval getMTRelLen() {
	    tassertMsg(isMountPoint(), "woops\n");
	    return mountPointInfo->getRelLen();
	}
	ObjectHandle *getMTOH() {
	    tassertMsg(isMountPoint(), "woops\n");
	    return mountPointInfo->getOH();
	}
	char *getSLData() {
	    tassertMsg(isSymLink(), "woops\n");
	    return symbolicLinkInfo->getData();
	}
	char *getDesc() {
	    tassertMsg(isMountPoint(), "woops\n");
	    return mountPointInfo->getDesc();
	}
	uval getDescLen() {
	    tassertMsg(isMountPoint(), "woops\n");
	    return mountPointInfo->getDescLen();
	}
	uval getIsBind() {
	    tassertMsg(isMountPoint(), "woops\n");
	    return mountPointInfo->getIsBind();
	}

	SysStatus makeMountPoint(ObjectHandle poh,
				 const PathName *pth, uval pthLen,
				 const char *desc, uval descLen,
				 uval isCoverable, uval isBind);

	SysStatus overwriteMountPoint(ObjectHandle poh,
				      const PathName *pth, uval pthLen,
				      const char *desc, uval descLen,
				      uval isCoverable, uval isBind);

	SysStatus makeSymLink(const char *str, uval len);

	MountComponent *findChild(PathName *name, uval pathLen);

	void *nextChild(void *curr, MountComponent *&child) {
	    return children.next(curr, child);
	}

	void removeChild(MountComponent *child) {
	    uval tmp = children.remove(child);
	    tassert( (tmp==1), err_printf("no such child\n"));
	}

	uval compare(PathName *pth, uval pathLen) {
	    if (pth->matchComp(pathLen, comp, compLen-1)) { return 1; }
	    return 0;
	}

	uval getDents(struct direntk42 *buf, uval len);

	void print(uval indent);
	void printMtab(char *basePath, VisibilityType type,
		       uval upMPUncoverable);

	SysStatus getNameTreeList(
	    ListSimple<ObjectHandle*, AllocGlobal> *&list);

	SysStatus init(char *pth, uval len);
	SysStatus reInit(char *pth, uval len);

	~MountComponent() {
	    if (!isEmpty()) {
		if (isMountPoint()) {
		    delete mountPointInfo;
		    mountPointInfo = NULL;
		} else {
		    tassertMsg(isSymLink(), "woops??\n");
		    delete symbolicLinkInfo;
		    symbolicLinkInfo = NULL;
		}
	    }
	    freeGlobal(comp, compLen);
	}

	static MountComponent *Create(PathName *pth, uval pthLen,
				      SysStatus &rc);
    };
public:
    // class used to marshal information into a buffer
    class MarshBuf {
	void *data;			// buffer used for marshalling data
	uval len;			// length of buffer
	uval used;			// amount of buffer actually used
	void copyToBuf(void *data, uval len);
	void *copyFromBuf(void *cur, void *p, uval len);
	void addPath(PathName *pth, uval pthLen);
    public:
	void init();
	void init(void *buf, uval l, uval us) {data = buf; len = l; used = us;}
	void reset();
	void addEntry(PathName *pth, uval pthLen, ObjectHandle *oh,
		      PathName *relPath, uval relLen, char *desc,
		      uval descLen, uval isCov, uval isBind);

	// returns NULL if no next entry, cur is NULL to start with
	void *getEntry(void *cur, PathName *&pth, uval &pthLen,
		       ObjectHandle &oh, PathName *&relPath, uval &lenRP,
		       char *desc, uval &lenDesc,
		       uval &isCoverable, uval &isBind);

	// on server side to copy into client buffer
	uval copyToClientBuf(uval inlen, void *buf, uval &cur, uval &left);
    };

protected:
    MountComponent top;

    MountComponent *addMountComponent(const PathName *mountPath, uval lenMP);
public:
    void print();			// for debugging

    DEFINE_NOOP_NEW(MountPointMgrCommon);
    void init();

    // free everything and re-initialize
    void reInit();

    // marshal state into the specified buffer
    SysStatus marshalToBuf(MarshBuf *marshBuf);

    // initialize class from buffer
    SysStatus demarshalFromBuf(MarshBuf *marshBuf);

    // on return:
    // unRes - points to part of name not yet resolved
    // unResLen - points to part of length not yet resolved
    // mc - points to the mount component
    SysStatus lookup(const PathName *name, uval namelen,
		     PathName *&unRes, uval &unResLen,
		     MountComponent *&mc, uval followLink = 1);

    // repeatedly calls above handling symbolic links, stoping
    // when it gets to a mountpoint
    // on return:
    // name - holds the new path after handling symlinks
    // namelen - is new length of path
    // unRes - points to part of name not yet resolved
    // unResLen - points to part of length not yet resolved
    // mc - points to the mount component
    SysStatus lookupMP(PathName *name, uval &namelen, uval maxLen,
		       PathName *&unRes, uval &unResLen,
		       MountComponent *&mc, uval followLink = 1);

    // calls lookupMP to perform lookup that handles symlinks
    // and just prepares return values for parameters
    // on return:
    // name - holds the new path to go to server
    // namelen - is new length of path
    // unRes - points to part of name not yet resolved
    // unResLen - points to part of length not yet resolved
    // oh - is the object handle identifies server
    SysStatus lookup(PathName *name, uval &namelen, uval maxlen,
		     PathName *&unRes, uval &unResLen,
		     ObjectHandle &oh, uval followLink = 1);

    /*
     * registers a new mount point:
     * mountPath - path we are mounting at
     * oh        - name tree (served by a file server) to be mounted
     * relPath   - pathname of mount point in server *relative* to the root
     *             of the name tree being mounted (the absolute pathname of
     *             mount point in server is already being stored in the name
     *             tree root)
     * desc      - string providing textual description of the mount point.
     *             This is used by printMtab.
     * descLen   - length of string desc
     */
    SysStatus registerMountPoint(const PathName *mountPath, uval lenMP,
				 ObjectHandle oh,
				 const PathName *relPath, uval lenRP,
				 const char *desc, uval descLen,
				 uval isCoverable,
				 uval isBind = 0);

    // resolve and cache any symbolic links in this path
    SysStatus resolveSymbolicLink(const PathName *pth, uval pthLen);

    /*
     * registers a new symbolic link:
     * symPath - path we are puttin the link at
     * buf     - the contents of the symbolic link
     */
    SysStatus registerSymLink(const PathName *symPath, uval lenSP,
			      char *buf, uval lenBuf);

    SysStatus bind(const PathName *oldPath, uval oldLen,
		   const PathName *newPath, uval newLen,
		   uval isCoverable);

    /*
     * get directory entries from some path,
     * for now, no cookie, i.e., everything must fit in one buffer
     * no . or .., i.e., only files really there if they exist
     */
    SysStatusUval getDents(PathName *name, uval namelen,
			   struct direntk42 *buf, uval len);

    SysStatus printMtab();

    // support to iterate over mounted file systems.
    // The 'list' parameter will return a list of object holder
    // for NameTrees currently mounted. It's responsability of the caller
    // to free the list after usage
    SysStatus getNameTreeList(ListSimple<ObjectHandle*, AllocGlobal> *&list);

    static void Test();
};
#endif /* #ifndef __MOUNT_POINT_MGR_COMMON_H_ */
