/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mount.x,v 1.4 2000/05/11 11:30:09 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  
 * **************************************************************************/

const MNTPATHLEN = 1024;
const MNTNAMELEN = 255;
const FHSIZE = 32;

typedef opaque dhandle[FHSIZE];
typedef string dirpath<MNTPATHLEN>;
typedef string name<MNTNAMELEN>;    

typedef struct mountnode *mountlist;
typedef struct groupnode *grouplist;
typedef struct exportnode *exportlist;  

struct mountnode {
  name hostname;
  dirpath directory;
  mountlist nextentry;
};

struct groupnode {
  name grname;
  grouplist grnext;
};

struct exportnode {
  dirpath filesys;
  grouplist groups;
  exportlist next;
};

union fhstatus switch(unsigned status)
{
case 0:
  dhandle directory;
default:
  void;
};     

program MOUNTPROG {
  version MOUNTVERS {
    void MOUNTPROC_NULL(void) = 0;            /* Not used */
    fhstatus MOUNTPROC_MNT(dirpath) = 1;
    mountlist MOUNTPROC_DUMP(void) = 2;
    void MOUNTPROC_UMNT(dirpath) = 3;
    void MOUNTPROC_UMNTALL(void) = 4;
    exportlist MOUNTPROC_EXPORT(void) = 5;
  } = 1;
} = 100005;
   



