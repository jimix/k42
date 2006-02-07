# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.49 2005/06/13 19:29:02 apw Exp $
# ############################################################################

#debug trace for MKANCHOR setting
#foo:=$(shell echo MKANCHOR in lib is $(MKANCHOR) >/dev/tty)

SUBDIRS = gcc libc lk
K42LIB_ADDR=0x100070000000
K42ALIAS_ADDR=0x100050000000

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

RELOCATE=$(TOOLBIN_PRE)relocate

ifdef CURR_DIR
$(MKKITCHTOP)/lib/$(CURR_DIR):
	@$(ECHO) "creating library dir: $@"
	$(INSTALL) --mode 0775 -d $@
create_obj_directory:: $(MKKITCHTOP)/lib/$(CURR_DIR)
else
create_obj_directory::
	@$(ECHO) "bogus creating directory:" $(CURR_DIR)
endif

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

LIBSRCS +=
TARGETS +=

clean::
	cd $(LIBDIR) && $(RM) *.a crt0s.o crt0c.o


.PHONY: dynlinker install_dynlinker

dynlinker: install_dynlinker

install_dynlinker:	$(MKKITCHROOT)/klib/exec.so \
			$(MKKITCHROOT)/klib/libk42sys.so.1 \
			$(MKKITCHROOT)/klib/libk42sys.so

$(MKKITCHROOT)/klib/libk42sys.so.1: libk42sys.so.1 
	$(INSTALL) --mode 0755 $< $@
	$(INSTALL_DSK)

$(MKKITCHROOT)/klib/libk42sys.so: libk42sys.so.1
	$(INSTALL) --mode 0755 $< $@
	$(INSTALL_DSK)

$(MKKITCHROOT)/klib/exec.so: exec.so 
	$(INSTALL) --mode 0755 $< $@
	$(INSTALL_DSK)

_%.map: $(LIBDIR)/%.a
	$(TARGET_OBJDUMP)  -w  -t $^ | \
	egrep ' [gwO] '| egrep -v 'UND' | egrep -v '@@' | egrep -v 'savef' |\
	sed -e 's/^.*[[:space:]][[:space:]]*\([@\._a-zA-Z0-9]*\)$$/  \1;/g'>$@

k42sys.map: _libemu.map _libbase.map
	echo 'K42_0.0.1 {' >$@
	echo '	global:' >>$@
	cat $^ |sort >>$@
	echo '  local:' >>$@
	echo '	*;' >>$@
	echo '};' >>$@
	echo 'GLIBC_2.2.5 { _sys_nerr; };' >>$@

#This object file, when linked in last, forces the plt section to have
#space allocated for it in the library file
 plt.o:
	echo '.section .plt,"a"' | $(TARGET_AS) - -o plt.o

CRT0 =/home/mostrows/elf.k42/install/lib/powerpc/partDeb/crt0c.o

# Generate the linker script from one of ld's default scripts
k42sys.scp: elf64ppc.xsc Makefile
	sed -e 's:\. = 0 + SIZEOF:\. = $(K42LIB_ADDR) + SIZEOF:' \
	    -e  's/ALIGN (0x10000)/ALIGN (0x10000000)/' < $< > $@

glibc_subset.a: glibc_subset.symbols $(KITCH_GLIBC)
	mkdir tmp
	sed -e 's/[[:space:]]*#.*$$//' -e '/^$$/d' $< | sort > tmp/symbols
	$(TARGET_NM) -a $(KITCH_GLIBC) | \
	    $(AWK) '/^.*:$$/ {file = $$1} / [WD] / {print $$3, file}' | \
	    sort | \
	    join tmp/symbols - | \
	    sed -e 's/.* \(.*\):$$/\1/' | \
	    sort -u > tmp/objfiles
	(cd tmp; \
	    $(TARGET_AR) xv $(KITCH_GLIBC) $$(cat objfiles); \
	    $(TARGET_AR) rv ../$@ $$(cat objfiles); \
	)
	$(RM) -rf tmp

k42sys.o: $(LIB_BASE) $(LIB_EMU) $(LIBDIR)/ProgExecUsr.o glibc_subset.a
	$(TARGET_CC) -nostdlib -nostartfiles -r -o $@		\
	    -Wl,--whole-archive $(LIB_BASE) $(LIB_EMU) $(LIBDIR)/ProgExecUsr.o \
	    -Wl,--no-whole-archive glibc_subset.a
	@echo "$(TARGET_NM) -p $@ | egrep -w U"
	@if $(TARGET_NM) -p $@ | egrep -w U; then \
	    echo "Error:  undefined symbols found in $@"; \
	    echo "$(RM) $@"; \
	    $(RM) $@; \
	    false; \
	fi

.INTERMEDIATE: libk42sys.so.1.tmp

# Create a library and then relocate it to the known load address
# runProcessCommon will load this library for all apps 
# (except resMgrServer and baseServers)
exec.so: k42sys.o k42sys.scp k42sys.map plt.o
	$(TARGET_CC) -nostdlib -nostartfiles -shared		\
	-o libk42sys.so.1.tmp					\
	k42sys.o plt.o						\
	-Wl,-e,_k42start					\
	-Wl,-Bsymbolic						\
	-Wl,-z,combreloc					\
	-Wl,--version-script=k42sys.map				\
	-Wl,-T,k42sys.scp					\
	-Wl,-Map,k42sys.loadmap					\
	-Wl,-soname=libk42sys.so.1
	$(RELOCATE) -i libk42sys.so.1.tmp -o $@ -l $(K42LIB_ADDR)
	$(STRIP.dbg)

# This creates the library to be loaded by ld.so to access k42 interfaces
# When this library is loaded, it will actually make ld.so link to the already
# loaded exec.so, so only 4K of data from this file is actually loaded into
# the application.  mkreflib does magic on the elf headers to make this work.
libk42sys.so.1: exec.so
	$(RELOCATE) -i $< -o $@ -r -l $(K42ALIAS_ADDR)
	$(RELOCATE) -i $<.dbg -o $@.dbg -r -l $(K42ALIAS_ADDR)


# ############################################################################
#  end of object directory rules
# ############################################################################
endif
