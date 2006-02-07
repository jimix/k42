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

#include "_eh.h"

extern void *__builtin_dwarf_cfa(void);
extern void *__builtin_return_address(int);
extern void *__builtin_extract_return_address(void *);
extern void __builtin_eh_return (void *, long, void *);


typedef struct old_exception_table {
    void *start_region;
    void *end_region;
    void *exception_handler;
} old_exception_table;

typedef struct exception_table {
    void *start_region;
    void *end_region;
    void *exception_handler;
    void *match_info;
} exception_table;

typedef struct exception_lang_info {
    short language;
    short version;
} exception_lang_info;

typedef struct exception_descriptor {
    void *runtime_id_field;
    exception_lang_info lang;
    exception_table table[1];
} exception_descriptor;



short
__get_eh_table_version(exception_descriptor *table)
{
    return table->lang.version;
}

short
__get_eh_table_language(exception_descriptor *table)
{
    return table->lang.language;
}

static void *
old_find_exception_handler(void *pc, old_exception_table *table)
{
    if (table) {
	int pos;
	int best = -1;

	for (pos = 0; table[pos].start_region != (void *) -1; ++pos) {
	    if (table[pos].start_region <= pc && table[pos].end_region > pc) {
		if (best == -1 ||
		    (table[pos].end_region <= table[best].end_region &&
		     table[pos].start_region >= table[best].start_region)) {
		    best = pos;
		}
	    } else if (best >= 0 && table[pos].start_region > pc) {
		break;
	    }
	}
	if (best != -1) {
	    return table[best].exception_handler;
	}
    }
    return (void *)0;
}

static void *
find_exception_handler(void *pc, exception_descriptor *table,
		       __eh_info *eh_info, int rethrow, int *cleanup)
{
    void *retval = ((void *)0) ;
    *cleanup = 1;
    if (table) {
	int pos = 0;

	exception_table *tab = &(table->table[0]);

	if (rethrow) {
	    pos = ((exception_table *) pc) - tab;
	    pc = ((exception_table *) pc)->end_region - 1;
	    if (tab[pos].start_region != (void *) -1) {
		pos++;
	    }
        } else {
	    pc--;
	}

	for ( ; tab[pos].start_region != (void *) -1; pos++) {
	    if (tab[pos].start_region <= pc && tab[pos].end_region > pc) {
		if (tab[pos].match_info) {
		    __eh_matcher matcher = eh_info->match_function;

		    if (matcher) {
			void *ret = (*matcher)((void *) eh_info,
					       tab[pos].match_info, table);
			if (ret) {
			    if (retval == ((void *)0) ) {
				retval = tab[pos].exception_handler;
			    }
			    *cleanup = 0;
			    break;
			}
                    }
                } else {
		    if (retval == ((void *)0)) {
			retval = tab[pos].exception_handler;
		    }
                }
            }
        }
    }
    return retval;
}

typedef struct frame_state {
    void *cfa;
    void *eh_ptr;
    long cfa_offset;
    long args_size;
    long reg_or_offset[76 +1];
    unsigned short cfa_reg;
    unsigned short retaddr_column;
    char saved[76 +1];
} frame_state;

#if FRAME

struct object {
    void *pc_begin;
    void *pc_end;
    struct dwarf_fde *fde_begin;
    struct dwarf_fde **fde_array;
    size_t count;
    struct object *next;
};

extern void __register_frame (void * );
extern void __register_frame_table (void *);
extern void __deregister_frame (void *);
extern void __register_frame_info (void *, struct object *);
extern void __register_frame_info_table (void *, struct object *);
extern void *__deregister_frame_info (void *);
#endif /* #if FRAME */
extern struct frame_state *__frame_state_for (void *, struct frame_state *);


typedef int ptr_type __attribute__ ((mode (pointer)));
typedef int word_type __attribute__ ((mode (__word__)));

static __inline__  int in_reg_window(int reg, frame_state *udata) {return 0;}

static word_type *
get_reg_addr(unsigned reg, frame_state *udata, frame_state *sub_udata)
{
    while (udata->saved[reg] == 2 ) {
	reg = udata->reg_or_offset[reg];
	if (in_reg_window (reg, udata)) {
	    udata = sub_udata;
	    sub_udata = ((void *)0);
	}
    }
    if (udata->saved[reg] == 1 ) {
	return (word_type *)(udata->cfa + udata->reg_or_offset[reg]);
    } else {
	abort();
    }
}

static __inline__  void *
get_reg(unsigned reg, frame_state *udata, frame_state *sub_udata)
{
    return (void *)(ptr_type) *get_reg_addr (reg, udata, sub_udata);
}

static __inline__  void
put_reg(unsigned reg, void *val, frame_state *udata)
{
    *get_reg_addr (reg, udata, ((void *)0) ) = (word_type)(ptr_type) val;
}

static void
copy_reg(unsigned reg, frame_state *udata, frame_state *target_udata)
{
    word_type *preg = get_reg_addr (reg, udata, ((void *)0) );
    word_type *ptreg = get_reg_addr (reg, target_udata, ((void *)0) );

    memcpy(ptreg, preg, __builtin_dwarf_reg_size (reg));
}

static __inline__  void *
get_return_addr(frame_state *udata, frame_state *sub_udata)
{
    void *retvalue;

    retvalue = __builtin_extract_return_addr(
	get_reg(udata->retaddr_column, udata, sub_udata));
    return(retvalue);
}



static __inline__  void
put_return_addr(void *val, frame_state *udata)
{
    val = __builtin_frob_return_addr (val);
    put_reg (udata->retaddr_column, val, udata);
}




static void *
next_stack_level(void *pc, frame_state *udata, frame_state *caller_udata)
{
    caller_udata = __frame_state_for (pc, caller_udata);
    if (! caller_udata) {
	return 0;
    }

    if (udata->saved[caller_udata->cfa_reg]) {
	caller_udata->cfa = get_reg (caller_udata->cfa_reg, udata, 0);
    } else {
	caller_udata->cfa = udata->cfa;
    }
    caller_udata->cfa += caller_udata->cfa_offset;

    return caller_udata;
}

void
__unwinding_cleanup()
{
}

static void *
throw_helper(struct eh_context *eh, void *pc,
	     frame_state *my_udata, long *offset_p)
{
    frame_state ustruct2, *udata = &ustruct2;
    frame_state ustruct;
    frame_state *sub_udata = &ustruct;
    void *saved_pc = pc;
    void *handler;
    void *handler_p = NULL;
    void *pc_p = NULL;
    frame_state saved_ustruct;
    int new_eh_model;
    int cleanup = 0;
    int only_cleanup = 0;
    int rethrow = 0;
    int saved_state = 0;
    long args_size;
    __eh_info *eh_info = (__eh_info *)eh->info;

    if (eh->table_index != (void *) 0) {
	rethrow = 1;
    }
    memcpy(udata, my_udata, sizeof (*udata));

    handler = (void *) 0;
    for (;;) {
	frame_state *p = udata;
	udata = next_stack_level (pc, udata, sub_udata);
	sub_udata = p;

	if (! udata) {
	    break;
	}

	if (udata->eh_ptr == ((void *)0) ) {
	    new_eh_model = 0;
	} else {
	    new_eh_model = (((exception_descriptor *)(udata->eh_ptr))->
			    runtime_id_field == ((void *) -2) );
	}

	if (rethrow) {
	    rethrow = 0;
	    handler = find_exception_handler (eh->table_index, udata->eh_ptr,
					      eh_info, 1, &cleanup);
	    eh->table_index = (void *)0;
        } else {
	    if (new_eh_model) {
		handler = find_exception_handler (pc, udata->eh_ptr, eh_info,
						  0, &cleanup);
	    } else {
		handler = old_find_exception_handler (pc, udata->eh_ptr);
	    }
	}

	if (handler) {
	    if (cleanup) {
		if (!saved_state) {
		    saved_ustruct = *udata;
		    handler_p = handler;
		    pc_p = pc;
		    saved_state = 1;
		    only_cleanup = 1;
		}
	    } else {
		only_cleanup = 0;
		break;
	    }
	}
	pc = get_return_addr (udata, sub_udata) - 1;
    }

    if (saved_state) {
	udata = &saved_ustruct;
	handler = handler_p;
	pc = pc_p;
	if (only_cleanup) {
	    __unwinding_cleanup ();
	}
    }

    if (! handler) {
	__terminate();
    }
    eh->handler_label = handler;

    args_size = udata->args_size;

    if (pc == saved_pc) {
        udata = my_udata;
    } else {
	int i;
	void *handler_pc = pc;

	pc = saved_pc;
	memcpy (udata, my_udata, sizeof (*udata));

	while (pc != handler_pc) {
	    frame_state *p = udata;
	    udata = next_stack_level (pc, udata, sub_udata);
	    sub_udata = p;

	    for (i = 0; i < 76 ; ++i) {
		if (i != udata->retaddr_column && udata->saved[i]) {
		    if (in_reg_window (i, udata)
			&& udata->saved[udata->retaddr_column] == 2
			&& udata->reg_or_offset[udata->retaddr_column] == i) {
			continue;
		    }
		    copy_reg (i, udata, my_udata);
		}
	    }
	    pc = get_return_addr (udata, sub_udata) - 1;
	}

	if (udata->saved[udata->retaddr_column] == 2 ) {
	    i = udata->reg_or_offset[udata->retaddr_column];
	    if (in_reg_window (i, udata)) {
		copy_reg (i, udata, my_udata);
	    }
	}
    }
    *offset_p = udata->cfa - my_udata->cfa + args_size;
    return handler;
}

void
__throw()
{
    struct eh_context *eh = (*get_eh_context) ();
    void *pc, *handler;
    long offset;

    frame_state my_ustruct, *my_udata = &my_ustruct;

    if (! eh->info) {
	__terminate ();
    }

label:
    my_udata = __frame_state_for (&&label, my_udata);
    if (! my_udata) {
	__terminate ();
    }

    my_udata->cfa = __builtin_dwarf_cfa ();

    __builtin_unwind_init ();

    pc = __builtin_extract_return_addr(__builtin_return_address(0)) - 1;

    handler = throw_helper (eh, pc, my_udata, &offset);
  __builtin_eh_return ((void *)eh, offset, handler);
}

void
__rethrow(void *index)
{
    struct eh_context *eh = (*get_eh_context) ();
    void *pc, *handler;
    long offset;

    frame_state my_ustruct, *my_udata = &my_ustruct;

    if (! eh->info) {
	__terminate ();
    }

    eh->table_index = index;
label:
    my_udata = __frame_state_for (&&label, my_udata);
    if (! my_udata) {
	__terminate ();
    }
    my_udata->cfa = __builtin_dwarf_cfa ();
    __builtin_unwind_init ();
    pc = __builtin_extract_return_addr (__builtin_return_address (0)) - 1;
    handler = throw_helper (eh, pc, my_udata, &offset);
    __builtin_eh_return ((void *)eh, offset, handler);
}
