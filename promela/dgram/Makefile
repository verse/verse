#
# $Id: Makefile 701 2010-11-17 21:49:34Z jiri $
#

SPIN=spin
CC=gcc
CFLAGS=-DNP -DBITSTATE
#CFLAGS=-DBITSTATE -DSAFETY
SIMFLAGS=-s -m
# -s print send messages
# -m do not block, when channel is full; instead, message is dropped
VERFLAGS=-m -a 

DGRAM_SOURCE=dgram.pml
DGRAM_BIN=dgram

sim:
	${SPIN} ${SIMFLAGS} ${DGRAM_SOURCE}

# Generate source code of verifier to the file pan.c
pan.c: ${DGRAM_SOURCE}
	${SPIN} ${VERFLAGS} $<
# Compile verifier
${DGRAM_BIN}: pan.c
	${CC} -o $@ ${CFLAGS} $<
# Do own verification
ver: ${DGRAM_BIN}
	./${DGRAM_BIN} -l

clean:
	rm -f pan.* ${DGRAM_BIN} 