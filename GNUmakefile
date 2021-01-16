UNAME_S:=	$(shell uname -s)

ifeq ($(UNAME_S),Darwin)
SUFFIX:=	dylib
CFLAGS+=	-arch x86_64
else
SUFFIX:=	so
endif

CFLAGS+=	-fPIC
LIBS+=		-lsqlite3

all: sqlite-phhammdist.$(SUFFIX) sqlite-phhammdist_debug.$(SUFFIX)

sqlite-phhammdist.$(SUFFIX): sqlite-phhammdist.c GNUmakefile
	$(CC) -shared $(CFLAGS) -o $@ $< $(LIBS)

sqlite-phhammdist_debug.$(SUFFIX): sqlite-phhammdist.c GNUmakefile
	$(CC) -DDEBUG -shared $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f sqlite-phhammdist.$(SUFFIX)