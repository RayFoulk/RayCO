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
AUX_SRCS := $(notdir $(shell find ./test -follow -name '*.c' -not -name 'test*'))
AUX_OBJS := $(patsubst %.c,%.o,$(AUX_SRCS))
VPATH     += $(TEST_DIRS)

STATIC_LIB  := $(PROJECT).a
SHARED_LIB  := $(PROJECT).so.0
SHARED_LINK := $(PROJECT).so

AR      := ar
CC      := gcc
BIN     := /usr/local/bin
LIB     := /usr/local/lib
CFLAGS  := $(PROJ_INCL) -Wall -pipe -std=c99 -fPIC
LDFLAGS := -lc -pie -shared

.PHONY: all
all: CFLAGS += -O2 -fomit-frame-pointer
all: $(STATIC_LIB) $(SHARED_LIB)

.PHONY: debug
debug: CFLAGS += -O0 -g -D BLAMMO_ENABLE -fmax-errors=3
debug: $(STATIC_LIB) $(SHARED_LIB)

$(STATIC_LIB): $(PROJ_OBJS)
	$(AR) rcs $(STATIC_LIB) $(PROJ_OBJS)

$(SHARED_LIB): $(PROJ_OBJS)
	$(CC) $(CFLAGS) -shared -Wl,-soname,$(SHARED_LIB) -o $(SHARED_LIB) $(PROJ_OBJS) $(LDFLAGS)

.PHONY: test
test: CFLAGS += $(TEST_INCL) -O0 -g -D BLAMMO_ENABLE -fmax-errors=3 -Wno-unused-label -fprofile-arcs -ftest-coverage
test: $(TEST_BINS) 

#$(TEST_BINS) : $(TEST_OBJS) $(PROJ_OBJS)
#	$(CC) $(CFLAGS) -o $(TEST_BINS) $(TEST_OBJS) $(PROJ_OBJS) $(LDFLAGS)

test_bytes : test_bytes.o $(AUX_OBJS) $(PROJ_OBJS)
	$(CC) $(CFLAGS) -o test_bytes test_bytes.o $(AUX_OBJS) $(PROJ_OBJS) $(LDFLAGS)

test_chain : test_chain.o $(AUX_OBJS) $(PROJ_OBJS)
	$(CC) $(CFLAGS) -o test_chain test_chain.o $(AUX_OBJS) $(PROJ_OBJS) $(LDFLAGS)


.PHONY: notabs
notabs:
	find . -type f -regex ".*\.[ch]" -exec sed -i -e "s/\t/    /g" {} +

.PHONY: clean
clean:
	rm -f core *.gcno $(TEST_OBJS) $(TEST_BINS)
	rm -f $(PROJ_OBJS) $(STATIC_LIB) $(SHARED_LIB) $(SHARED_LINK)
