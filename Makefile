CC := mwcceppc.exe
AS := mwasmeppc.exe

GAMESOURCE := ./GameSource
PULSAR := ./PulsarEngine
SECURE := ../rr-secure
KAMEK := Kamek.exe
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

EXTERNALS := -externals=$(GAMESOURCE)/symbols.txt -externals=$(GAMESOURCE)/anticheat.txt -externals=$(SECURE)/anticheat.txt -versions=$(GAMESOURCE)/versions.txt

# Source files
CPP_SRCS := $(shell find $(PULSAR) -type f -name "*.cpp")
ASM_SRCS := $(shell find $(PULSAR) -type f -name "*.S")
SECURE_CPP_SRCS := $(shell find $(SECURE) -type f -name "*.cpp")
SECURE_ASM_SRCS := $(shell find $(SECURE) -type f -name "*.S")

# Object files
CPP_OBJS := $(patsubst $(PULSAR)/%.cpp, build/%.o, $(CPP_SRCS))
ASM_OBJS := $(patsubst $(PULSAR)/%.S, build/%.o, $(ASM_SRCS))
SECURE_CPP_OBJS := $(patsubst $(SECURE)/%.cpp, build/secure/%.o, $(SECURE_CPP_SRCS))
SECURE_ASM_OBJS := $(patsubst $(SECURE)/%.S, build/secure/%.o, $(SECURE_ASM_SRCS))

OBJS := $(CPP_OBJS) $(ASM_OBJS) $(SECURE_CPP_OBJS) $(SECURE_ASM_OBJS)

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

# External security sources
build/secure/%.o: $(SECURE)/%.cpp | build
	@echo Compiling Secure C++ $<...
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c -o $@ $<

build/secure/%.o: $(SECURE)/%.S | build
	@echo Assembling Secure $<...
	@mkdir -p $(dir $@)
	@$(AS) $(ASFLAGS) -o $@ $<

build/Network/RoomKey.o: .force

.force:

force_link: build/kamek.o build/RuntimeWrite.o $(OBJS)
	@echo Linking...
	@$(KAMEK) $^ -dynamic $(EXTERNALS) -output-combined=build/Code.pul -output-map=build/Code.map

install: force_link
	@echo Copying binaries to $(RIIVO)/Binaries...
	@mkdir -p $(RIIVO)/Binaries
	@cp build/Code.pul $(RIIVO)/Binaries

clean:
	@echo Cleaning...
	@rm -rf build