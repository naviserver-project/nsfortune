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
MODOBJS     = nsfortune.o

include  $(NAVISERVER)/include/Makefile.module

