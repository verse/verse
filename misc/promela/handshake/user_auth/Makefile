SPIN=spin
CC=gcc
#CFLAGS=-DNP
CFLAGS=-DSAFETY
SIMFLAGS=-s -r
VERFLAGS=-a

AUTH_SOURCE=user_auth.pml
AUTH_BIN=user_auth

sim:
	${SPIN} ${SIMFLAGS} ${AUTH_SOURCE}

# Generate source code of verifier to the file pan.c
pan.c: ${AUTH_SOURCE}
	${SPIN} ${VERFLAGS} $<
# Compile verifier
${AUTH_BIN}: pan.c
	${CC} -o $@ ${CFLAGS} $<
# Do own verification
ver: ${AUTH_BIN}
	./${AUTH_BIN} #-l

clean:
	rm -f pan.* ${AUTH_BIN} 