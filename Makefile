PROJECT := librayco

PROJ_SRCS := $(notdir $(shell find ./src -follow -name '*.c'))
PROJ_DIRS := $(sort $(dir $(shell find ./src -follow -name '*.c')))
PROJ_OBJS := $(patsubst %.c,%.o,$(PROJ_SRCS))
PROJ_INCL := $(patsubst %,-I%,$(PROJ_DIRS))
VPATH     := $(PROJ_DIRS)

TEST_SRCS := $(notdir $(shell find ./test -follow -name 'test_*.c'))
TEST_DIRS := $(sort $(dir $(shell find ./test -follow -name 'test_*.c')))
TEST_OBJS := $(patsubst %.c,%.o,$(TEST_SRCS))
TEST_BINS := $(patsubst %.c,%,$(TEST_SRCS))
TEST_INCL := $(patsubst %,-I%,$(TEST_DIRS))

# TODO: AUX SOURCES

STATIC_LIB  := $(PROJECT).a
SHARED_LIB  := $(PROJECT).so.0
SHARED_LINK := $(PROJECT).so

AR      := ar
CC      := gcc
BIN     := /usr/local/bin
LIB     := /usr/local/lib
CFLAGS  := $(PROJ_INCL) -Wall -pipe -std=c99 -fPIC
LDFLAGS := -lc -pie -shared

all: CFLAGS += -O2 -fomit-frame-pointer
all: $(STATIC_LIB) $(SHARED_LIB)

debug: CFLAGS += -O0 -g -D BLAMMO_ENABLE -fmax-errors=3
debug: $(STATIC_LIB) $(SHARED_LIB)

$(STATIC_LIB): $(PROJ_OBJS)
	$(AR) rcs $(STATIC_LIB) $(PROJ_OBJS)

$(SHARED_LIB): $(PROJ_OBJS)
	$(CC) $(CFLAGS) -shared -Wl,-soname,$(SHARED_LIB) -o $(SHARED_LIB) $(PROJ_OBJS) $(LDFLAGS)

test: CFLAGS += $(TEST_INCL) -O0 -g -D BLAMMO_ENABLE -fmax-errors=3 -fprofile-arcs -ftest-coverage
test: $(TEST_BINS)

$(TEST_BINS) : $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $(TEST_BINS) $(PROJ_OBJS) $(LDFLAGS)






notabs:
	find . -type f -regex ".*\.[ch]" -exec sed -i -e "s/\t/    /g" {} +

clean:
	rm -f core $(PROJ_OBJS) $(STATIC_LIB) $(SHARED_LIB) $(SHARED_LINK)
