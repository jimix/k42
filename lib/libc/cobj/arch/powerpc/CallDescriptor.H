/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CallDescriptor.H,v 1.1 2004/01/24 20:58:16 bob Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: arbiter clustered object
 * **************************************************************************/

#ifndef __CALL_DESCRIPTOR_H_
#error This file should not be included directly; only through non-arch header.
#endif

class CallDescriptor : public CallDescriptorGeneric{
protected:
    uval truebottomSP;
    uval bottomSP;
    uval CORCT_PTR;
    uval startSP;
    double paramFP12;
    double paramFP11;
    double paramFP10;
    double paramFP9;
    double paramFP8;
    double paramFP7;
    double paramFP6;
    double paramFP5;
    double paramFP4;
    double paramFP3;
    double paramFP2;
    double paramFP1;
    double paramFP0;
    uval backchain;
    uval cr;
    uval lr;
    uval compiler;
    uval binder;
    uval toc;
    uval paramInt0;
    uval paramInt1;
    uval paramInt2;
    uval paramInt3;
    uval paramInt4;
    uval paramInt5;
    uval paramInt6;
    uval paramInt7;


public:
    virtual void setIntArg(uval argno, uval value){
        switch(argno){
            case 0:
                paramInt0 = value;
                return;
            case 1:
                paramInt1 = value;
                return;
            case 2:
                paramInt2 = value;
                return;
            case 3:
                paramInt3 = value;
                return;
            case 4:
                paramInt4 = value;
                return;
            case 5:
                paramInt5 = value;
                return;
            case 6:
                paramInt6 = value;
                return;
            case 7:
                paramInt7 = value;
                return;
        }
        err_printf("tried to access too high a param number on ppc\n");
    }

    virtual uval getIntArg(uval argno){
        switch(argno){
            case 0:
                return paramInt0;
            case 1:
                return paramInt1;
            case 2:
                return paramInt2;
            case 3:
                return paramInt3;
            case 4:
                return paramInt4;
            case 5:
                return paramInt5;
            case 6:
                return paramInt6;
            case 7:
                return paramInt7;
        }
        err_printf("tried to access too high a param number on ppc\n");
        return 0;
    }

    virtual void setFPArg(uval argno, double value){
        switch(argno){
            case 0:
                paramFP0 = value;
                return;
            case 1:
                paramFP1 = value;
                return;
            case 2:
                paramFP2 = value;
                return;
            case 3:
                paramFP3 = value;
                return;
            case 4:
                paramFP4 = value;
                return;
            case 5:
                paramFP5 = value;
                return;
            case 6:
                paramFP6 = value;
                return;
            case 7:
                paramFP7 = value;
                return;
            case 8:
                paramFP8 = value;
                return;
            case 9:
                paramFP9 = value;
                return;
            case 10:
                paramFP10 = value;
                return;
            case 11:
                paramFP11 = value;
                return;
            case 12:
                paramFP12 = value;
                return;
        }
        err_printf("tried to access too high a param number on ppc\n");
    }

    virtual double getFPArg(uval argno){
        switch(argno){
            case 0:
                return paramFP0;
            case 1:
                return paramFP1;
            case 2:
                return paramFP2;
            case 3:
                return paramFP3;
            case 4:
                return paramFP4;
            case 5:
                return paramFP5;
            case 6:
                return paramFP6;
            case 7:
                return paramFP7;
            case 8:
                return paramFP8;
            case 9:
                return paramFP9;
            case 10:
                return paramFP10;
            case 11:
                return paramFP11;
            case 12:
                return paramFP12;
        }
        err_printf("tried to access too high a param number on ppc\n");
        return 0;
    }
};
