/* Copyright (C) 2000, 2001
   Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifndef ___EH_H_
#define ___EH_H_

#include <stdlib.h>
#include <string.h>

struct eh_context
{
    void *handler_label;
    void **dynamic_handler_chain;
    void *info;
    void *table_index;
};

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

typedef void *(*__eh_matcher)(void *, void *, void *);

extern void __default_terminate(void) /*__attribute__ ((__noreturn__))*/;
extern void __terminate();
//extern void (*__terminate_func)();
extern void __throw();
extern void *__throw_type_match(void *, void *, void *);
extern void __empty();
extern void *__get_eh_context();
extern void **__get_eh_info();
extern void ***__get_dynamic_handler_chain();
extern void __sjthrow(void) /*__attribute__ ((__noreturn__))*/;
extern void __sjpopnthrow(void) /*__attribute__ ((__noreturn__))*/;
extern int __eh_rtime_match(void *rtime);
extern struct eh_context *(*get_eh_context)();
extern struct eh_context *eh_context_static();
extern struct eh_context *eh_context_initialize();
#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */
typedef struct __eh_info
{
    __eh_matcher match_function;
    short language;
    short version;
} __eh_info;


#endif /* #ifndef ___EH_H_ */
