CC := G:/Coding/MarioKart/Compilers/Wii/1.7/mwcceppc.exe
AS := G:/Coding/MarioKart/Compilers/Wii/1.7/mwasmeppc.exe

GAMESOURCE := ./GameSource
PULSAR := ./PulsarEngine
KAMEK_H := ./KamekInclude

ifneq ($(filter install%,$(MAKECMDGOALS)),)
PULSAR_RANDOM_KEY := $(shell python -c "import random; print(hex(random.randint(0, 0xFFFFFFFF)))")
$(info [PULSAR] Random Room Key generated: $(PULSAR_RANDOM_KEY))
else
PULSAR_RANDOM_KEY := 0xADD2BFAF
endif

-include .env

CFLAGS := -I- -i $(KAMEK_H) -i $(GAMESOURCE) -i $(PULSAR) -opt all -inline auto -enum int -proc gekko -fp hard -sdata 0 -sdata2 0 -maxerrors 1 -func_align 4 -DPULSAR_RANDOM_KEY=$(PULSAR_RANDOM_KEY) $(CFLAGS)
ASFLAGS := -proc gekko -c

EXTERNALS := -externals=$(GAMESOURCE)/symbols.txt -externals=$(GAMESOURCE)/anticheat.txt -versions=$(GAMESOURCE)/versions.txt

# Source files (PowerShell-friendly; emit forward-slash paths for make)
CPP_SRCS := $(shell powershell -NoProfile -Command "$$root = Resolve-Path '$(PULSAR)'; Get-ChildItem -Path $$root -Recurse -Filter '*.cpp' | ForEach-Object { ('$(PULSAR)/' + $$_.FullName.Substring($$root.Path.Length + 1)).Replace('\','/') }")
ASM_SRCS := $(shell powershell -NoProfile -Command "$$root = Resolve-Path '$(PULSAR)'; Get-ChildItem -Path $$root -Recurse -Filter '*.S' | ForEach-Object { ('$(PULSAR)/' + $$_.FullName.Substring($$root.Path.Length + 1)).Replace('\','/') }")

# Object files
CPP_OBJS := $(patsubst $(PULSAR)/%.cpp, build/%.o, $(CPP_SRCS))
ASM_OBJS := $(patsubst $(PULSAR)/%.S, build/%.o, $(ASM_SRCS))

OBJS := $(CPP_OBJS) $(ASM_OBJS)

all: build force_link

.PHONY: all force_link clean test

test:
	@echo "CPP sources:"
	@echo "$(CPP_SRCS)"
	@echo "ASM sources:"
	@echo "$(ASM_SRCS)"

build:
	@powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path 'build' | Out-Null"

build/kamek.o: $(KAMEK_H)/kamek.cpp | build
	@$(CC) $(CFLAGS) -c -o $@ $<

build/RuntimeWrite.o: $(KAMEK_H)/RuntimeWrite.cpp | build
	@echo Compiling $<...
	@$(CC) $(CFLAGS) -c -o $@ $<

# C++ compilation
build/%.o: $(PULSAR)/%.cpp | build
	@echo Compiling C++ $<...
	@powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(dir $@)' | Out-Null"
	@$(CC) $(CFLAGS) -c -o $@ $<

# Assembly compilation (.S)
build/%.o: $(PULSAR)/%.S | build
	@echo Assembling $<...
	@powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(dir $@)' | Out-Null"
	@$(AS) $(ASFLAGS) -o $@ $<

build/Network/RoomKey.o: .force

.force:

force_link: build/kamek.o build/RuntimeWrite.o $(OBJS)
	@echo Linking...
	@$(KAMEK) $^ -dynamic $(EXTERNALS) -output-combined=build/Code.pul -output-map=build/Code.map

install: force_link
	@echo Copying binaries to $(RIIVO)/Binaries...
	@powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(RIIVO)/Binaries' | Out-Null"
	@powershell -NoProfile -Command "Copy-Item -Force 'build/Code.pul' '$(RIIVO)/Binaries'"

installCT: force_link
	@echo Copying binaries to $(RIIVO)/CT/Binaries...
	@powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(RIIVO)/CT/Binaries' | Out-Null"
	@powershell -NoProfile -Command "Copy-Item -Force 'build/Code.pul' '$(RIIVO)/CT/Binaries'"

clean:
	@echo Cleaning...
	@powershell -NoProfile -Command "Remove-Item -Recurse -Force -ErrorAction SilentlyContinue 'build'"
