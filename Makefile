ifndef NAVISERVER
    NAVISERVER  = /usr/local/ns
endif

#
# Module name
#
MOD      =  nsfortune.so

#
# Objects to build.
#
OBJS     = nsfortune.o

include  $(NAVISERVER)/include/Makefile.module

