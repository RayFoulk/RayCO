PROJECT := rayco
SOURCES := $(notdir $(shell find ./src -follow -name '*.c'))
SRCDIRS := $(sort $(dir $(shell find ./src -follow -name '*.c')))
OBJECTS := $(patsubst %.c,%.o,$(SOURCES))
INCLUDE += $(patsubst %,-I%,$(SRCDIRS))
VPATH   := $(SRCDIRS)

CC      := gcc
BIN     := /usr/local/bin
LIB     := /usr/local/lib
CFLAGS  := $(INCLUDE) -Wall -pipe -std=c99 -Wno-unused-label -fPIC
LDFLAGS := -lc -pie

all: CFLAGS += -O2 -fomit-frame-pointer
all: $(PROJECT)


debug: CFLAGS += -O0 -g -D BLAMMO_ENABLE -fmax-errors=3
debug: $(OBJECTS)
	$(CC) $(CFLAGS) -o $(PROJECT)_debug $(OBJECTS) $(LDFLAGS)

test: debug
	./$(PROJECT)_debug

$(PROJECT): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(PROJECT) $(OBJECTS) $(LDFLAGS)

install: $(PROJECT)
	cp -f $(PROJECT) $(LIB)

uninstall:
	rm -f $(LIB)/$(PROJECT)

notabs:
	find . -type f -regex ".*\.[ch]" -exec sed -i -e "s/\t/    /g" {} +

clean:
	rm -f core *.o *.a $(PROJECT) $(PROJECT)_debug

