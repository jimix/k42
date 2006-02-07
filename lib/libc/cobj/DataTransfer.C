/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DataTransfer.C,v 1.2 2001/06/26 15:03:48 jimix Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implementation of the cobj data transfer support
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <cobj/CObjRoot.H>
#include <cobj/DataTransfer.H>

// const DTTypeSet DTTSNULL;

/*static*/ DTType
DataTransferObject::negotiate(CObjRoot *oldRoot, CObjRoot *newRoot)
{
    DTTypeSet xSet, iSet;
    
    oldRoot->getDataTransferExportSet(&xSet);
    newRoot->getDataTransferImportSet(&iSet);

    return xSet.matchBest(iSet);
}
