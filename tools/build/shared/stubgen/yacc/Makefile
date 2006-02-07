#	@(#)Makefile	5.3 (Berkeley) 5/12/90

export SGI_ABI=
export TOOLROOT=
export COMP_TARGET_ROOT=

CC = gcc
HOST_CFLAGS = -g -MD

PROG=	yacc
SRCS=	closure.c error.c lalr.c lr0.c main.c mkpar.c output.c reader.c \
	skeleton.c symtab.c verbose.c warshall.c
OBJS=	closure.o error.o lalr.o lr0.o main.o mkpar.o output.o reader.o \
	skeleton.o symtab.o verbose.o warshall.o
MAN1=	yacc.0 yyfix.0

programs :: $(PROG)

relink::
	-$(RM) $(PROG) && $(MAKE) programs


$(PROG) : $(OBJS)
	$(HOST_CC) $(HOST_CFLAGS) -o $(PROG) $(OBJS)

clean::
	-$(RM) $(OBJS) $(PROG)

