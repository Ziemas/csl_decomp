# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2005, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

IOP_BIN = csl-play.irx
IOP_OBJS = csl.o modmidi/src/modmidi.o modhsyn/src/modhsyn.o modhsyn/src/sdwrap.o
IOP_LIBS =
IOP_TABS = stdio.tab libsd.tab ioman.tab thbase.tab sysmem.tab sysclib.tab timrman.tab thsemap.tab modhsyn.tab modmidi.tab

IOP_INCS += -Imodmidi/include/ -Imodhsyn/include/ -Icommon/include/
IOP_CFLAGS += -Wall -fno-common -Wdeclaration-after-statement -Werror-implicit-function-declaration -std=gnu11

EE_BIN = main.elf
EE_OBJS = main.o
EE_LIBS = -ldebug

all: $(EE_BIN) $(IOP_BIN)

%.adp : %.wav
	$(PS2SDK)/bin/adpenc -L $< $@


#$(PS2SDK)/bin/adpenc -L $< $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS) $(IOP_BIN) $(IOP_OBJS)

run: $(IOP_BIN)
	ps2client execiop host:$(IOP_BIN)

sim: $(IOP_BIN)
	PCSX2 --elf="hello.elf"

#PCSX2 --irx=$(IOP_BIN)

reset:
	ps2client reset

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
include $(PS2SDK)/samples/Makefile.eeglobal
