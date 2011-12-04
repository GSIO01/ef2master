COMPILE_PLATFORM=$(shell uname|sed -e s/_.*//|tr '[:upper:]' '[:lower:]')
COMPILE_ARCH=$(shell uname -m | sed -e s/i.86/i386/)

##### Win32 variables #####

WIN32_EXE=ef2master.exe
WIN32_CFLAGS=-D_WIN32_WINNT=0x0501
WIN32_LDFLAGS=-lws2_32
WIN32_RM=del

##### Unix variables #####

UNIX_EXE=ef2master.$(COMPILE_ARCH)
UNIX_LDFLAGS=
UNIX_RM=rm -f

##### Common variables #####

CC=gcc
CFLAGS_COMMON=-Wall
CFLAGS_DEBUG=$(CFLAGS_COMMON) -g
CFLAGS_RELEASE=$(CFLAGS_COMMON) -O2 -DNDEBUG
OBJECTS=common.o ef2master.o games.o messages.o servers.o system.o

##### Commands #####

help:
	@echo
	@echo "===== Choose one ====="
	@echo "* $(MAKE) help          : this help"
	@echo "* $(MAKE) debug         : make debug binaries"
	@echo "* $(MAKE) release       : make release binaries"
	@echo "* $(MAKE) clean         : delete all files produced by a build"
	@echo "* $(MAKE) mingw-debug   : make debug binaries using MinGW"
	@echo "* $(MAKE) mingw-release : make release binaries using MinGW"
	@echo "* $(MAKE) win-clean     : delete all files produced by a build (for Windows)"
	@echo

.c.o:
	$(CC) $(CFLAGS) -c $*.c

$(EXE): $(OBJECTS)
ifeq ($(COMPILE_PLATFORM), mingw32)
	$(CC) -o $@ $(OBJECTS) icon.o $(LDFLAGS)
else
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)
endif

debug:
	$(MAKE) EXE=$(UNIX_EXE) LDFLAGS="$(UNIX_LDFLAGS)" CFLAGS="$(CFLAGS_DEBUG)" $(UNIX_EXE) 

mingw-debug:
	windres -i icon.rc -o icon.o
	$(MAKE) EXE=$(WIN32_EXE) LDFLAGS="$(WIN32_LDFLAGS)" CFLAGS="$(WIN32_CFLAGS) $(CFLAGS_DEBUG)" $(WIN32_EXE)

release:
	$(MAKE) EXE=$(UNIX_EXE) LDFLAGS="$(UNIX_LDFLAGS)" CFLAGS="$(CFLAGS_RELEASE)" $(UNIX_EXE) 
	strip $(UNIX_EXE)

mingw-release:
	windres -i icon.rc -o icon.o
	$(MAKE) EXE=$(WIN32_EXE) LDFLAGS="$(WIN32_LDFLAGS)" CFLAGS="$(WIN32_CFLAGS) $(CFLAGS_RELEASE)" $(WIN32_EXE)
	strip $(WIN32_EXE)

clean:
	-$(UNIX_RM) $(WIN32_EXE)
	-$(UNIX_RM) $(UNIX_EXE)
	-$(UNIX_RM) *.o *~

win-clean:
	-$(WIN32_RM) $(WIN32_EXE)
	-$(WIN32_RM) *.o
