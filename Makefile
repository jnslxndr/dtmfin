#### Simple Makefile for Simple Projects ####

# Just declare the Project name
PRODUCT = dtmfosc

SRC=dtmfin

LIBS=-lportaudio

BIN_DIR=bin
OUT = $(BIN_DIR)/$(PRODUCT)

REMOVE = rm -rf

# LINKED_LIBS = -L$(wildcard deps/**/lib)

OSC_LIB=./lib/oscpack_1_1_0

INCLUDES=-I$(BIN_DIR) -I$(OSC_LIB)

LDFLAGS=$(LINKED_LIBS) $(LIBS) -lstdc++

CFLAGS=$(INCLUDES) -Wall -Wstrict-prototypes -std=c99
# CPPFLAGS=

OSC_SOURCES=$(wildcard $(OSC_LIB)/osc/*.cpp) $(wildcard $(OSC_LIB)/ip/posix/*.cpp) $(wildcard $(OSC_LIB)/ip/*.cpp)

CPPFILES=$(wildcard $(SRC)/*.cpp) $(OSC_SOURCES)
CCFILES=$(wildcard $(SRC)/*.cc)
CFILES=$(wildcard $(SRC)/*.c)


OBJECTS=$(CFILES:.c=.o) \
	$(CPPFILES:.cpp=.o)\
	$(CCFILES:.cc=.o) \
	$(CCFILES:.m=.o)

.PHONY: clean all

default: all

all: $(PRODUCT)

test: clean all
	./$(BIN_DIR)/$(PRODUCT)

clean:
	$(REMOVE) $(OUT)
	$(REMOVE) $(OBJECTS)

$(PRODUCT): $(OBJECTS)
	mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $(BIN_DIR)/$(PRODUCT) $(OBJECTS)

#### Generating object files ####
# object from C
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

# object from C++ (.cc, .cpp)
.cc.o .cpp.o :
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@
