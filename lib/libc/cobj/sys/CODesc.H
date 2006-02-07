#ifndef __CO_DESC_H__
#define __CO_DESC_H__
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CODesc.H,v 1.2 2005/04/15 17:39:33 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/****************************************************************************
 * Module Description: 
 * **************************************************************************/

class CODesc {
    uval ref;
    uval root;
    uval typeToken;
public:
    uval getRef()                      { return ref;           }
    void setRef(uval _ref)              { ref = _ref;      }
    uval getRoot()                     { return root;          }
    void setRoot(const uval _root)      { root = _root;    }
    uval getTypeToken()                { return typeToken;     }
    void setTypeToken(const uval tt)   { this->typeToken = tt; } 
};

class COTypeDesc {
    enum { STRINGLEN=72 };
    uval typeToken;
    char string[STRINGLEN];
public:
    uval  getTypeToken()               { return typeToken;      }
    void  setTypeToken(const uval tt)  { this->typeToken =  tt; }
    char *getString()                  { return string;         }
    void  setSting(char *string) { strncpy(this->string, string, STRINGLEN); }
};

#if 0
class CODescList {
    CODesc *descs;
    uval   maxNumDesc;
    uval   numValidDesc;
    uval   grow(uval newMax);
    reset();
    load();
public:
    CODesc *nextOnVP(uval *cur);
};
#endif

#endif /* __CO_DESC_H__ */
