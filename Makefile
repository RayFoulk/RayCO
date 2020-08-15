PROJECT := raytl
SOURCES := $(notdir $(shell find . -follow -name '*.c'))
SRCDIRS := $(sort $(dir $(shell find . -follow -name '*.c')))
OBJECTS := $(patsubst %.c,%.o,$(SOURCES))
INCLUDE += $(patsubst %,-I%,$(SRCDIRS))
VPATH   := $(SRCDIRS)

CC      := gcc
BIN	:= /usr/local/bin
LIB     := /usr/local/lib
CFLAGS	:= $(INCLUDE) -Wall -pipe -std=c99 -Wno-unused-label -fPIC
LDFLAGS	:= -lc -pie

all: CFLAGS += -O2 -fomit-frame-pointer
all: $(PROJECT)

$(PROJECT): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(PROJECT) $(OBJECTS) $(LDFLAGS)

debug: CFLAGS += -O0 -g -D BLAMMO_ENABLE
debug: $(OBJECTS)
	$(CC) $(CFLAGS) -o $(PROJECT)_debug $(OBJECTS) $(LDFLAGS)

install: $(PROJECT)
	cp -f $(PROJECT) $(BIN)

uninstall:
	rm -f $(BIN)/$(PROJECT)

clean:
	rm -f core *.o *.a $(PROJECT) $(PROJECT)_debug

test: debug
	./$(PROJECT)_debug
