UNAME_S:=	$(shell uname -s)

ifeq ($(OS),Windows_NT)
SUFFIX:=	dll
EXE:=		.exe
else ifeq ($(UNAME_S),Darwin)
SUFFIX:=	dylib
# Universal binary so it loads in both arm64 and x86_64 processes.
ARCHFLAGS:=	-arch arm64 -arch x86_64
else
SUFFIX:=	so
PICFLAGS:=	-fPIC
endif

# Use a SQLite installed somewhere other than the default search paths, e.g.
#   make test SQLITE_PREFIX=$(brew --prefix sqlite)
# macOS's system libsqlite3 cannot load extensions, so `make test` needs this
# there.
SQLITE_PREFIX?=
ifneq ($(SQLITE_PREFIX),)
CPPFLAGS+=	-I$(SQLITE_PREFIX)/include
TEST_LDFLAGS+=	-L$(SQLITE_PREFIX)/lib -Wl,-rpath,$(SQLITE_PREFIX)/lib
endif

WARNFLAGS:=	-std=c99 -Wall -Wextra -Wpedantic

# Loadable extensions get every SQLite API call through pApi, so they must
# not link against libsqlite3.
CFLAGS+=	$(WARNFLAGS) $(ARCHFLAGS) $(PICFLAGS) -fvisibility=hidden

# Only the test driver links against libsqlite3.
TEST_LIBS:=	-lsqlite3
SANFLAGS:=	-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all -g

EXT:=		sqlite-phhammdist.$(SUFFIX)
TEST_BIN:=	tests/test_phhammdist$(EXE)
# Built in subdirectories so SQLite derives the same entry point name.
SWAR_EXT:=	tests/swar/$(EXT)
ASAN_EXT:=	tests/asan/$(EXT)
ASAN_BIN:=	tests/asan/test_phhammdist$(EXE)

.PHONY: all clean test test-asan test-valgrind

all: $(EXT) sqlite-phhammdist_debug.$(SUFFIX)

$(EXT): sqlite-phhammdist.c GNUmakefile
	$(CC) -shared $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LIBS)

# Debug build: writes argument values to stderr on every call.
# For local debugging only -- never deploy sqlite-phhammdist_debug.$(SUFFIX).
sqlite-phhammdist_debug.$(SUFFIX): sqlite-phhammdist.c GNUmakefile
	$(CC) -DPHHAMMDIST_DEBUG -shared $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LIBS)

# --- Tests -------------------------------------------------------------------

$(TEST_BIN): tests/test_phhammdist.c GNUmakefile
	$(CC) $(CPPFLAGS) $(WARNFLAGS) -o $@ $< $(TEST_LDFLAGS) $(TEST_LIBS)

# The portable SWAR popcount fallback, which GCC/Clang builds don't otherwise use.
$(SWAR_EXT): sqlite-phhammdist.c GNUmakefile
	@mkdir -p $(@D)
	$(CC) -DPHHAMMDIST_NO_BUILTIN_POPCOUNT -shared $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LIBS)

test: $(TEST_BIN) $(EXT) $(SWAR_EXT)
	./$(TEST_BIN) ./$(EXT)
	./$(TEST_BIN) ./$(SWAR_EXT)

$(ASAN_EXT): sqlite-phhammdist.c GNUmakefile
	@mkdir -p $(@D)
	$(CC) -shared $(CPPFLAGS) $(WARNFLAGS) $(PICFLAGS) $(SANFLAGS) -o $@ $< $(LIBS)

$(ASAN_BIN): tests/test_phhammdist.c GNUmakefile
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(WARNFLAGS) $(SANFLAGS) -o $@ $< $(TEST_LDFLAGS) $(TEST_LIBS)

test-asan: $(ASAN_BIN) $(ASAN_EXT)
	ASAN_OPTIONS=detect_leaks=1 ./$(ASAN_BIN) ./$(ASAN_EXT)

test-valgrind: $(TEST_BIN) $(EXT)
	valgrind --quiet --error-exitcode=1 --leak-check=full ./$(TEST_BIN) ./$(EXT)

clean:
	rm -f $(EXT) sqlite-phhammdist_debug.$(SUFFIX) $(TEST_BIN)
	rm -rf tests/swar tests/asan
