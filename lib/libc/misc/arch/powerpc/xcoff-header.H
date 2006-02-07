#ifndef __XCOFF_HEADER_H_
#define __XCOFF_HEADER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: xcoff-header.H,v 1.7 2001/10/05 21:48:09 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/


#define XCOFF64MAGIC   0757

#define XCOFF_TEXT	0x20
#define XCOFF_DATA	0x40
#define XCOFF_BSS	0x80


typedef struct fileInfo {
	unsigned short	magicNumb;
	unsigned short	numbSections;
	int		timeDate;
	long long	symTable;
	unsigned short	auxHdr;
	unsigned short	flags;
	int		sunbSyms;
};

typedef	struct aOutInfo {
	short	magic;
	short	versionStamp;
	unsigned int rsrvDbg;
	unsigned long long textStart;
	unsigned long long dataStart;
	unsigned long long toc;
	short	sectNumbEntry;
	short	sectNumbText;
	short	sectNumbData;
	short	sectNumbToc;
	short	sectNumbLoader;
	short	sectNumbBss;
	short	allignText;
	short	allignData;
	char    modType[2];
	unsigned char cpuFlag;
	unsigned char cpuType;
	int	resv2[1];
	long long	textSize;
	long long	dataSize;
	long long	bssSize;
	long long	entryPoint;
	unsigned long long maxStack;
	unsigned long long maxData;
	int		resv3[4];
};


struct xCoffHdr {
	struct fileInfo	fileInfo;
	struct aOutInfo	aOutInfo;
};


typedef struct sectionHdr {
	char			sextName[8];
	unsigned long long	paddr;
	unsigned long long	vaddr;
	unsigned long long	size;
	long long		sectPtr;
	long long		relPtr;
	long long		lineNumbPtr;
	unsigned int		numbReloc;
	unsigned int		numbLineEntries;
	int			flags;
};

#define SECTHDRSZ			sizeof(sectionHdr)

#endif /* #ifndef __XCOFF_HEADER_H_ */
