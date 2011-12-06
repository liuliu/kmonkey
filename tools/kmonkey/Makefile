##===- projects/sample/tools/sample/Makefile ---------------*- Makefile -*-===##

#
# Indicate where we are relative to the top of the source tree.
#
LEVEL = ../..

#
# Give the name of the tool.
#
TOOLNAME = kmonkey

LINK_COMPONENTS = linker asmparser bitwriter

#
# List libraries that we'll need
# We use LIBS because sample is a dynamic library.
#
USEDLIBS = kmonkey.a

NO_PEDANTIC = 1

#
# Include Makefile.common so we know what to do.
#
include $(LEVEL)/Makefile.common

LIBS += -lgsl -lblas
