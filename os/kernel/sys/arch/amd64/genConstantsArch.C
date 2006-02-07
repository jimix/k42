/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: genConstantsArch.C,v 1.12 2002/05/01 15:48:22 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *     generation of the machine-dependent assembler constants
 *     NOTE:  This file is a C fragment that is included directly in the
 *            machine-independent genConstants.C.
 * **************************************************************************/

// VVV

    FIELD_OFFSET1(EL, ExceptionLocal, trapVolatileState);

    FIELD_OFFSET2(PA, ProcessAnnex, machine, syscallFramePtr);

    FIELD_OFFSET1(VS, VolatileState, float_save);
    FIELD_OFFSET1(VS, VolatileState, rax);
    FIELD_OFFSET1(VS, VolatileState, rcx);
    FIELD_OFFSET1(VS, VolatileState, rdx);
    FIELD_OFFSET1(VS, VolatileState, rbp);
    FIELD_OFFSET1(VS, VolatileState, rsi);
    FIELD_OFFSET1(VS, VolatileState, rdi);
    FIELD_OFFSET1(VS, VolatileState, r8);
    FIELD_OFFSET1(VS, VolatileState, r9);
    FIELD_OFFSET1(VS, VolatileState, r10);
    FIELD_OFFSET1(VS, VolatileState, r11);
    FIELD_OFFSET2(VS, VolatileState, faultFrame, rip);
    FIELD_OFFSET2(VS, VolatileState, faultFrame, cs);
    FIELD_OFFSET2(VS, VolatileState, faultFrame, rflags);
    FIELD_OFFSET2(VS, VolatileState, faultFrame, rsp);
    FIELD_OFFSET2(VS, VolatileState, faultFrame, ss);

    ENUM_VALUE(CR0, CR0_amd64, PE_bit);
    ENUM_VALUE(CR0, CR0_amd64, WP_bit);
    ENUM_VALUE(CR0, CR0_amd64, PG_bit);
