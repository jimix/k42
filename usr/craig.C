/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: craig.C,v 1.3 2005/06/28 19:48:44 rosnbrg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/TypeMgr.H>
#include <cobj/TypeFactory.H>
#include <cobj/Example.H>
#include <sys/systemAccess.H>

int
main(char *argv[], int argc)
{
    NativeProcess();

    TypeID id;
    TypeFactory *factory;
    ExampleRef a, b, c;
    int i, j;

    // find the garbage type
    (TypeFactory *)DREFGOBJ(TheTypeMgrRef)->locateType("Example", id);

    // get the garbage factory
    factory = (TypeFactory *)DREFGOBJ(TheTypeMgrRef)->locateFactory("Example");

    // make some garbage
    factory->Create((uval *)&a);
    factory->Create((uval *)&b);
    factory->Create((uval *)&c);

    // use the garbage
    i = 5;
    j = 2;
    while (1) {
        err_printf("a: %d\n", DREF(a)->math(i, j));
        err_printf("b: %d\n", DREF(b)->math(i, j));
        err_printf("c: %d\n", DREF(c)->math(i, j));
    }
}
