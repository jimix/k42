/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: genConstantsArch.C,v 1.2 2001/10/19 20:02:24 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *     generation of the machine-dependent assembler constants
 *     NOTE:  This file is a C fragment that is included directly in the
 *            machine-independent genConstants.C.
 * **************************************************************************/

    FIELD_OFFSET2(PA, ProcessAnnex, machine, dispatcherPhysAddr);
    FIELD_OFFSET2(PA, ProcessAnnex, machine, excStatePhysAddr);
