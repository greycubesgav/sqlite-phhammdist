UNAME_S:=	$(shell uname -s)

ifeq ($(OS),Windows_NT)
SUFFIX:=	dll
else ifeq ($(UNAME_S),Darwin)
SUFFIX:=	dylib
# Universal binary so it loads in both arm64 and x86_64 processes.
CFLAGS+=	-arch arm64 -arch x86_64
else
SUFFIX:=	so
CFLAGS+=	-fPIC
endif

# Loadable extensions get every SQLite API call through pApi, so they must
# not link against libsqlite3.
CFLAGS+=	-std=c99 -Wall -Wextra -Wpedantic -fvisibility=hidden

all: sqlite-phhammdist.$(SUFFIX) sqlite-phhammdist_debug.$(SUFFIX)

sqlite-phhammdist.$(SUFFIX): sqlite-phhammdist.c GNUmakefile
	$(CC) -shared $(CFLAGS) -o $@ $< $(LIBS)

# Debug build: writes argument values to stderr on every call.
# For local debugging only -- never deploy sqlite-phhammdist_debug.$(SUFFIX).
sqlite-phhammdist_debug.$(SUFFIX): sqlite-phhammdist.c GNUmakefile
	$(CC) -DPHHAMMDIST_DEBUG -shared $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f sqlite-phhammdist.$(SUFFIX) sqlite-phhammdist_debug.$(SUFFIX)
