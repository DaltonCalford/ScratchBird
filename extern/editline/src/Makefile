# Copyright (C) 1994-2012 Free Software Foundation, Inc.

# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

# Alex Peshkov (peshkoff@mail.ru) 2013
# Simplified this makefile to make it better fit scratchbird project needs



build_triplet = x86_64-unknown-linux-gnu
host_triplet = x86_64-unknown-linux-gnu
append_1 = tokenizern.c historyn.c
#append_2 = strlcpy.c
#append_3 = strlcat.c
append_4 = vis.c
append_5 = unvis.c
append_6 = eln.c

CONFIG_HEADER = ../config.h

SOURCES_DIST = chared.c common.c el.c emacs.c hist.c \
	keymacro.c map.c chartype.c parse.c prompt.c read.c refresh.c \
	search.c sig.c terminal.c tty.c vi.c fgetln.c wcsdup.c \
	tokenizer.c history.c filecomplete.c readline.c chared.h el.h \
	hist.h histedit.h keymacro.h map.h chartype.h parse.h prompt.h \
	read.h refresh.h search.h sig.h sys.h terminal.h tty.h vis.h \
	filecomplete.h editline/readline.h strlcpy.c strlcat.c vis.c \
	unvis.c eln.c

#objects_1 = strlcpy.o
#objects_2 = strlcat.o
objects_3 = vis.o
objects_4 = unvis.o
objects_5 = eln.o
OBJECTS1 = chared.o common.o el.o emacs.o hist.o \
	keymacro.o map.o chartype.o parse.o prompt.o read.o \
	refresh.o search.o sig.o terminal.o tty.o vi.o fgetln.o \
	wcsdup.o tokenizer.o history.o filecomplete.o readline.o \
	$(objects_1) $(objects_2) $(objects_3) \
	$(objects_4) $(objects_5)
objects_6 = tokenizern.o historyn.o
objects_7 = fcns.o help.o $(objects_6)
OBJECTS2 = $(objects_7)
OBJECTS = $(OBJECTS1) $(OBJECTS2)

DEFAULT_INCLUDES = -I. -I..

COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(CPPFLAGS) $(CFLAGS)

SOURCES =
DIST_SOURCES = $(SOURCES_DIST)

OPTFLAGS = -O2
DEPFLAGS = -pipe -MMD -fPIC

AR = ar
AWK = mawk
CC = gcc
CCDEPMODE = depmode=gcc3
CFLAGS = 
CPP = gcc -E
CPPFLAGS = 
DUMPBIN = 
ECHO_C = 
ECHO_N = -n
ECHO_T = 
LD = /usr/bin/ld -m elf_x86_64
LDFLAGS = 
LIBOBJS = 
LIBS = -ltinfo 
LN_S = ln -s
MKDIR_P = /usr/bin/mkdir -p
NM = /usr/bin/nm -B
NMEDIT = 
OBJDUMP = objdump
OBJEXT = o
PACKAGE_VERSION = 3.1
PATH_SEPARATOR = :
RANLIB = ranlib
SED = /usr/bin/sed
SHELL = /bin/bash
STATICLIB_LINK = $(AR) crus
STRIP = strip
VERSION = 3.1

srcdir = .

BUILT_SOURCES = vi.h emacs.h common.h fcns.h help.h fcns.c help.c \
	$(append_1)
AHDR = vi.h emacs.h common.h
ASRC = $(srcdir)/vi.c $(srcdir)/emacs.c $(srcdir)/common.c
CLEANFILES = $(BUILT_SOURCES)
LIBRARY = libedit.a
libedit_la_SOURCES = chared.c common.c el.c emacs.c hist.c keymacro.c \
	map.c chartype.c parse.c prompt.c read.c refresh.c search.c \
	sig.c terminal.c tty.c vi.c fgetln.c wcsdup.c tokenizer.c \
	history.c filecomplete.c readline.c chared.h el.h hist.h \
	histedit.h keymacro.h map.h chartype.h parse.h prompt.h read.h \
	refresh.h search.h sig.h sys.h terminal.h tty.h vis.h \
	filecomplete.h editline/readline.h $(append_2) \
	$(append_3) $(append_4) $(append_5) \
	$(append_6)

nobase_include_HEADERS = histedit.h editline/readline.h
nodist_libedit_la_SOURCES = $(BUILT_SOURCES)

editlinelibrary:	all

all: $(BUILT_SOURCES)
	$(MAKE) libedit.a

.SUFFIXES:
.SUFFIXES: .c .o

$(LIBRARY): $(OBJECTS)
	-$(RM) $@
	$(STATICLIB_LINK) $@ $^

.c.o:
	$(COMPILE) $(DEPFLAGS) $(OPTFLAGS) -c $< -o $@

clean:
	rm -f $(CLEANFILES) $(OBJECTS) *.d

distclean:	clean
	rm -f $(LIBRARY) Makefile ../Makefile

.PHONY: editlinelibrary all all-am check check-am clean clean-generic \
	clean-libLTLIBRARIES clean-libtool cscopelist ctags distclean \
	distclean-compile distclean-generic distclean-libtool \
	distclean-tags distdir dvi dvi-am html html-am info info-am \
	install install-am install-data install-data-am install-dvi \
	install-dvi-am install-exec install-exec-am install-html \
	install-html-am install-info install-info-am \
	install-libLTLIBRARIES install-man \
	install-nobase_includeHEADERS install-pdf install-pdf-am \
	install-ps install-ps-am install-strip installcheck \
	installcheck-am installdirs maintainer-clean \
	maintainer-clean-generic mostlyclean mostlyclean-compile \
	mostlyclean-generic mostlyclean-libtool pdf pdf-am ps ps-am \
	tags uninstall uninstall-am uninstall-libLTLIBRARIES \
	uninstall-nobase_includeHEADERS


vi.h: Makefile $(srcdir)/vi.c
	AWK=$(AWK) sh $(srcdir)/makelist -h $(srcdir)/vi.c > $@

emacs.h: Makefile $(srcdir)/emacs.c
	AWK=$(AWK) sh $(srcdir)/makelist -h $(srcdir)/emacs.c > $@

common.h: Makefile $(srcdir)/common.c
	AWK=$(AWK) sh $(srcdir)/makelist -h $(srcdir)/common.c > $@

fcns.h: Makefile $(AHDR)
	AWK=$(AWK) sh $(srcdir)/makelist -fh $(AHDR) > $@

help.h: Makefile $(ASRC)
	AWK=$(AWK) sh $(srcdir)/makelist -bh $(ASRC) > $@

fcns.c: Makefile $(AHDR)
	AWK=$(AWK) sh $(srcdir)/makelist -fc $(AHDR) > $@

help.c: Makefile $(ASRC)
	AWK=$(AWK) sh $(srcdir)/makelist -bc $(ASRC) > $@

tokenizern.c: Makefile $(srcdir)/tokenizer.c
	AWK=$(AWK) sh $(srcdir)/makelist -n $(srcdir)/tokenizer.c > $@

historyn.c: Makefile $(srcdir)/history.c
	AWK=$(AWK) sh $(srcdir)/makelist -n $(srcdir)/history.c > $@

Dependencies = $(OBJECTS:.o=.d)
-include $(Dependencies)

# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:

