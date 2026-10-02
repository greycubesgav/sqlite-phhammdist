UNAME_S:=	$(shell uname -s)

ifeq ($(UNAME_S),Darwin)
SUFFIX:=	dylib
CFLAGS+=	-arch x86_64
else
SUFFIX:=	so
endif

CFLAGS+=	-std=c99 -Wall -Wextra -Wpedantic -fPIC
LIBS+=		-lsqlite3

all: sqlite-phhammdist.$(SUFFIX) sqlite-phhammdist_debug.$(SUFFIX)

sqlite-phhammdist.$(SUFFIX): sqlite-phhammdist.c GNUmakefile
	$(CC) -shared $(CFLAGS) -o $@ $< $(LIBS)

# Debug build: writes argument values to stderr on every call.
# For local debugging only -- never deploy sqlite-phhammdist_debug.$(SUFFIX).
sqlite-phhammdist_debug.$(SUFFIX): sqlite-phhammdist.c GNUmakefile
	$(CC) -DPHHAMMDIST_DEBUG -shared $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f sqlite-phhammdist.$(SUFFIX) sqlite-phhammdist_debug.$(SUFFIX)
