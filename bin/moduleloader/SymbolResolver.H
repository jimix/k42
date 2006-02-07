/******************************************************************************
* K42: (C) Copyright IBM Corp. 2005.
* All Rights Reserved
*
* This file is distributed under the GNU LGPL. You should have
* received a copy of the license along with K42; see the file LICENSE.html
* in the top-level directory for more details.
*
* $Id: SymbolResolver.H,v 1.1 2005/06/07 03:46:39 jk Exp $
*****************************************************************************/

#ifndef __SYMBOLRESOLVER_H_
#define __SYMBOLRESOLVER_H_

/**
 * A class to lookup symbols for object relocation
 */
class SymbolResolver {

public:

    struct syment {
	char *name;
	uval value;
	uval funcaddr;
	uval r2value;
    };

    DEFINE_GLOBAL_NEW(SymbolResolver);

    /**
     * Create a Resolver.
     */
    SymbolResolver();

    /**
     * Initialise the resolver. This will load the symbol table, and must be
     * called before resolve().
     */
    int init();

    /**
     * Return the address of a symbol, or null if the symbol is undefined
     * @param symbol The name of the symbol to resolve
     * @return The address of the symbol
     */
    struct syment *resolve(const char *symbol);

private:

    struct syment *symbols;
    unsigned int n_symbols;
    unsigned int alloc_symbols;

    int addSymbol(struct SymbolResolver::syment *syment);

    void dump(void);

};

#endif /* __SYMBOLRESOLVER_H_ */
