## -*- makefile -*-
##
## Process this file with automake to produce Makefile.in
##
## $Source:$
##
## $Id:$
##

VPATH = @srcdir@
INCLUDES=-I.
DEFS=@DEFS@

## Cannot use ansi2knr because it is under GPL
## AUTOMAKE_OPTIONS=ansi2knr
## config.guess and config.sub have special exceptions
AUTOMAKE_OPTIONS=foreign

bin_PROGRAMS = mkoccdeps

dist_man1_MANS = mkoccdeps.1

# Sources are all kept in libraries in sub-directories
mkoccdeps_SOURCES = mkoccdeps.c global.h support.h support.c

