#ifndef	GENCONSTDEFS_H
#define	GENCONSTDEFS_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: genConstDefs.C,v 1.5 2000/05/11 12:09:55 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *     define all functions that otherwise lead to genconstant link error
 * **************************************************************************/

SysStatus
BaseObj::destroyUnchecked()
{
    return(0);
}

#endif	// GENCONSTDEFS_H
