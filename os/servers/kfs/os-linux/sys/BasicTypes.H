#ifndef __BASIC_TYPES_H_
#define __BASIC_TYPES_H_

/*
 * K42 File System
 *
 * These are some basic macros/objects necessary for running KFS on Linux
 * Most of them were stolen from the K42 project and mildly modified.
 *
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: BasicTypes.H,v 1.9 2004/03/07 00:42:40 lbsoares Exp $
 */

#ifdef KFS_TOOLS
#include <kfsIncs.H>

#else /* #ifdef KFS_TOOLS */

extern "C" {
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/dirent.h>   /* for 'struct dirent' */
#include <linux/vfs.h>
#include <linux/string.h>
}

#include <sys/types.H>
#include <sys/SysStatus.H>
#include <sys/sysTypes.H>

extern "C" {
    void *allocGlobal(size_t size);
    void  freeGlobal(void *ptr);
    void  freeGlobalWithSize(void *ptr, int size);

    typedef unsigned short kdev_t;
    SysStatus LinuxBlockRead(void *data, kdev_t dev, int block, int size);
    SysStatus LinuxBlockWrite(void *data, kdev_t dev, int block, int size);
    int LinuxGetHardsectSize(kdev_t dev);
}

#define err_printf(stdargs...) printk(stdargs)

class AllocGlobal {
public:
    static void *alloc(uval size)
	{ return allocGlobal(size); }
    static void *alloc(uval size, uval &rsize)
	{ return allocGlobal(size); }
    static void  free(void *ptr, uval size)
	{ freeGlobalWithSize(ptr, size); }
    static void  free(void *ptr)
	{ freeGlobal(ptr); }
};

class AllocGlobalPadded {
public:
    static void *alloc(uval size)
	{ return allocGlobal(size); }
    static void *alloc(uval size, uval &rsize)
	{ return allocGlobal(size); }
    static void  free(void *ptr, uval size)
	{ freeGlobalWithSize(ptr, size); }
    static void  free(void *ptr)
	{ freeGlobal(ptr); }
};

#define DEFINE_NEW(CLASS,ALLOC)                                         \
inline void * operator new(size_t size)                                 \
{                                                                       \
    tassert(size==sizeof(CLASS),                                        \
             err_printf("Wrong new called for " #CLASS "\n"));          \
/*    err_printf("NEW! Class = " #CLASS " size=%d\n", size);              \ */ \
    return ALLOC::alloc(sizeof(CLASS));                                 \
}                                                                       \
inline void * operator new[](size_t size)                               \
{                                                                       \
/*    err_printf("NEW[]! Class = " #CLASS " size=%d\n", size);            \ */ \
    return ALLOC::alloc(size);                                          \
}                                                                       \
inline void operator delete(void * p)                                   \
{                                                                       \
/*    err_printf("DELETE! Class = " #CLASS " size=%d\n", sizeof(CLASS));  \ */ \
    return ALLOC::free(p, sizeof(CLASS));                               \
}                                                                       \
inline void operator delete[](void * p)                                 \
{                                                                       \
/*    err_printf("DELETE[]! Class = " #CLASS "\n");                       \ */ \
    return ALLOC::free(p);                                              \
}                                                                       \
inline void operator delete(void * p, uval size)                        \
{                                                                       \
    tassert(size==sizeof(CLASS),                                        \
            err_printf("Wrong delete called for" #CLASS "\n"));         \
/*    err_printf("DELETE(size)! Class = " #CLASS " size=%ld\n", size);    \ */ \
    ALLOC::free(p, sizeof(CLASS));                                      \
}                                                                       \
inline void operator delete[](void * p, uval size)                      \
{                                                                       \
/*    err_printf("DELETE[](size)! Class = " #CLASS " size=%ld\n", size);  \ */ \
    ALLOC::free(p, size);                                               \
}

#define DEFINE_GLOBAL_NEW(CLASS)       DEFINE_NEW(CLASS,AllocGlobal)
#define DEFINE_ALLOC_NEW(CLASS)        DEFINE_NEW(CLASS,ALLOC)
#define DEFINE_GLOBALPADDED_NEW(CLASS) DEFINE_NEW(CLASS,AllocGlobalPadded)

typedef uval      ThreadID;               // unique number identifies each thread
typedef uval      VPNum;                  // virtual processor number
typedef uval      XHandle;                // Extern handle of an internal ObjRef  
typedef uval      TypeID;                 // Unique TypeID of a Service/Obj Class
typedef uval64    SysTime;                // time in system dependent units
typedef uval      ObjectHandle;
typedef char      PathName;

typedef unsigned short          mode_t;
typedef long                    off_t; 

#define tassert(EX,STMT)                                                   \
    if (!(EX)) {                                                           \
	err_printf("ERROR: file \"%s\", line %d\n", __FILE__, __LINE__);   \
	STMT;                                                              \
	panic("\n");                                                       \
    }                                                                  

#define tassertMsg(EX, MSG...)                                             \
    if (!(EX)) {                                                           \
        err_printf(MSG);                                                   \
	panic("ERROR: file \"%s\", line %d\n", __FILE__, __LINE__);        \
    }                                                                  

#define tassertWrn(EX, MSG...)                                             \
    if (!(EX)) {                                                           \
        err_printf(MSG);                                                   \
	err_printf("WARNING: file \"%s\", line %d\n", __FILE__, __LINE__); \
    }

/* PANIC Assert */
#define passert(EX, STMT)                                                  \
    if (!(EX)) {                                                           \
	STMT;                                                              \
	panic("ERROR: file \"%s\", line %d\n", __FILE__, __LINE__);        \
    }                                                                  

#define passertMsg(EX, MSG...)                                             \
    if (!(EX)) {                                                           \
        err_printf(MSG) ;                                                  \
	panic("ERROR: file \"%s\", line %d\n", __FILE__, __LINE__);        \
    }                                                                  

#define passertWrn(EX, MSG...)                                             \
    if (!(EX)) {                                                           \
        err_printf(MSG);                                                   \
	panic("WARNING: file \"%s\", line %d\n", __FILE__, __LINE__);      \
    }

#define ALIGN_UP(addr,align)   (((uval)(addr) + ((align)-1)) & ~((align)-1))

/* Convert between stat structure types and directory types.  */
# define IFTODT(mode)   (((mode) & 0170000) >> 12)
# define DTTOIF(dirtype)        ((dirtype) << 12)

extern struct timeval xtime;
#define CURRENT_TIME (xtime.tv_sec)
#define time(x)  CURRENT_TIME

#define DREF(OBJ)       (*(OBJ))           // Normal clustered object deref

#define DEFINE_REFS(CLASS)                 \
     inline CLASS##Ref getRef() {          \
         return (CLASS##Ref)Obj::getRef(); \
     }

#endif /* KFS_TOOLS */
#endif /* __BASIC_TYPES_H_ */
