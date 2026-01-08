CC := mwcceppc.exe
AS := mwasmeppc.exe

GAMESOURCE := ./GameSource
PULSAR := ./PulsarEngine
KAMEK := Kamek.exe
KAMEK_H := ./KamekInclude

-include .env

CFLAGS := -I- -i $(KAMEK_H) -i $(GAMESOURCE) -i $(PULSAR) -opt all -inline auto -enum int -proc gekko -fp hard -sdata 0 -sdata2 0 -maxerrors 1 -func_align 4 $(CFLAGS)
ASFLAGS := -proc gekko -c

EXTERNALS := -externals=$(GAMESOURCE)/symbols.txt -externals=$(GAMESOURCE)/anticheat.txt -versions=$(GAMESOURCE)/versions.txt

# Source files
CPP_SRCS := $(shell find $(PULSAR) -type f -name "*.cpp")
ASM_SRCS := $(shell find $(PULSAR) -type f -name "*.S")

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
	@mkdir -p build

build/kamek.o: $(KAMEK_H)/kamek.cpp | build
	@$(CC) $(CFLAGS) -c -o $@ $<

build/RuntimeWrite.o: $(KAMEK_H)/RuntimeWrite.cpp | build
	@echo Compiling $<...
	@$(CC) $(CFLAGS) -c -o $@ $<

# C++ compilation
build/%.o: $(PULSAR)/%.cpp | build
	@echo Compiling C++ $<...
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $<

# Assembly compilation (.S)
build/%.o: $(PULSAR)/%.S | build
	@echo Assembling $<...
	@mkdir -p $(dir $@)
	@$(AS) $(ASFLAGS) -o $@ $<

force_link: build/kamek.o build/RuntimeWrite.o $(OBJS)
	@echo Linking...
	@$(KAMEK) $^ -dynamic $(EXTERNALS) -output-combined=build/Code.pul -output-map=build/Code.map

install: force_link
	@echo Copying binaries to $(RIIVO)/Binaries...
	@mkdir -p $(RIIVO)/Binaries
	@cp build/Code.pul $(RIIVO)/Binaries

installCT: force_link
	@echo Copying binaries to $(RIIVO)/CT/Binaries...
	@mkdir -p $(RIIVO)/CT/Binaries
	@cp build/Code.pul $(RIIVO)/CT/Binaries

clean:
	@echo Cleaning...
	@rm -rf build
