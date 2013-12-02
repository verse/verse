SPIN=spin
CC=gcc
CFLAGS=-DNP
#CFLAGS=-DSAFETY
SIMFLAGS=-s
VERFLAGS=-a

HOST_NEG_SOURCE=host_neg.pml
HOST_NEG_BIN=host_neg

sim:
	${SPIN} ${SIMFLAGS} ${HOST_NEG_SOURCE}

# Generate source code of verifier to the file pan.c
pan.c: ${HOST_NEG_SOURCE}
	${SPIN} ${VERFLAGS} $<
# Compile verifier
${HOST_NEG_BIN}: pan.c
	${CC} -o $@ ${CFLAGS} $<
# Do own verification
ver: ${HOST_NEG_BIN}
	./${HOST_NEG_BIN} -l

clean:
	rm -f pan.* ${HOST_NEG_BIN} 