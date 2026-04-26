# Build Loader.pul (channel bootstrap) for MKWii.
# Requires Metrowerks mwcceppc (set CC in ../.env like the main project).
# From repo root: make -C Loader
# From Loader/: make

# Parent of this Makefile (repo root), even when using make -C Loader.
REPO_ROOT := $(abspath ..)
-include $(REPO_ROOT)/.env

CC ?= mwcceppc.exe
# ../.env often has KAMEK := ./KamekLinker/Kamek.exe (relative to repo root, not Loader/).
KAMEK ?= $(REPO_ROOT)/KamekLinker/Kamek.exe
# .env paths are relative to repo root; this Makefile runs with cwd = Loader/.
ifneq ($(filter ./%,$(KAMEK)),)
override KAMEK := $(abspath $(REPO_ROOT)/$(patsubst ./%,%,$(KAMEK)))
endif

LOADER_BUILD := build
OBJS := $(LOADER_BUILD)/mkw.o $(LOADER_BUILD)/kamek.o

# Match Loader/build.bat include order (cwd = Loader for local includes).
CFLAGS := -I- \
	-i . \
	-i $(REPO_ROOT)/KamekInclude \
	-i $(REPO_ROOT)/GameSource \
	-i $(REPO_ROOT)/GameSource/MarioKartWii \
	-i $(REPO_ROOT)/PulsarEngine \
	-opt all -inline auto -enum int -proc gekko -fp hard \
	-sdata 0 -sdata2 0 -maxerrors 1 -func_align 4 \
	$(CFLAGS)

.PHONY: all clean

all: Loader.pul

Loader.pul: $(OBJS)
	@echo Linking Loader.pul...
	@"$(KAMEK)" $(OBJS) -static=0x80004000 -output-code=Loader.pul

$(LOADER_BUILD)/mkw.o: mkw.cpp | $(LOADER_BUILD)
	@echo Compiling $<...
	@"$(CC)" $(CFLAGS) -c -o $@ $<

$(LOADER_BUILD)/kamek.o: kamekLoader.cpp | $(LOADER_BUILD)
	@echo Compiling $<...
	@"$(CC)" $(CFLAGS) -c -o $@ $<

$(LOADER_BUILD):
	@mkdir -p $(LOADER_BUILD)
	

clean:
	$(RM) $(OBJS) Loader.pul
	@-rmdir $(LOADER_BUILD) 2>/dev/null || true
