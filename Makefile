PROJECT := librayco

PROJ_SRCS := $(notdir $(shell find ./src -follow -name '*.c'))
PROJ_DIRS := $(sort $(dir $(shell find ./src -follow -name '*.c')))
PROJ_OBJS := $(patsubst %.c,%.o,$(PROJ_SRCS))
PROJ_INCL := $(patsubst %,-I%,$(PROJ_DIRS))
VPATH     := $(PROJ_DIRS)

TEST_SRCS := $(notdir $(shell find ./test -follow -name 'test_*.c'))
TEST_DIRS := $(sort $(dir $(shell find ./test -follow -name 'test_*.c')))
TEST_OBJS := $(patsubst %.c,%.o,$(TEST_SRCS))
TEST_BINS := $(patsubst %.c,%.mut,$(TEST_SRCS))
TEST_INCL := $(patsubst %,-I%,$(TEST_DIRS))
AUX_SRCS := $(notdir $(shell find ./test -follow -name '*.c' -not -name 'test*'))
AUX_OBJS := $(patsubst %.c,%.o,$(AUX_SRCS))
VPATH     += $(TEST_DIRS)

STATIC_LIB  := $(PROJECT).a
SHARED_LIB  := $(PROJECT).so.0
SHARED_LINK := $(PROJECT).so

AR           := ar
LD           := ld
CC           := gcc
BIN          := /usr/local/bin
LIB          := /usr/local/lib
#CFLAGS       := $(PROJ_INCL) -Wall -pipe -std=c99 -fPIC -D_POSIX_C_SOURCE
CFLAGS       := $(PROJ_INCL) -Wall -pipe -fPIC
DEBUG_CFLAGS := -O0 -g -D BLAMMO_ENABLE -fmax-errors=3
ifeq ($(ANDROID_ROOT),)
LDFLAGS      := -lc -pie
COV_REPORT   := gcovr -r . --html-details -o coverage.html 
else
LDFLAGS      := -pie
COV_REPORT   :=
endif

.PHONY: all
all: CFLAGS += -O2 -fomit-frame-pointer
all: $(STATIC_LIB) $(SHARED_LIB)

.PHONY: debug
debug: CFLAGS += $(DEBUG_CFLAGS)
debug: $(STATIC_LIB) $(SHARED_LIB)

$(STATIC_LIB): $(PROJ_OBJS)
	$(AR) rcs $(STATIC_LIB) $(PROJ_OBJS)

$(SHARED_LIB): $(PROJ_OBJS)
	#$(LD) $(LDFLAGS) -shared -soname,$(SHARED_LIB) -o $(SHARED_LIB) $(PROJ_OBJS)
	$(LD) $(LDFLAGS) -shared -o $(SHARED_LIB) $(PROJ_OBJS)

.PHONY: test
test: CFLAGS += $(TEST_INCL) $(DEBUG_CFLAGS) -Wno-unused-label
ifeq ($(ANDROID_ROOT),)
test: CFLAGS += -fprofile-arcs -ftest-coverage
#test: LDFLAGS += -lgcov --coverage
endif
test: $(TEST_BINS)
	for testmut in test_*mut; do ./$$testmut; done
	$(COV_REPORT)

#$(LD) -o $@ $< $(AUX_OBJS) $(PROJ_OBJS) $(LDFLAGS)
test_%.mut : test_%.o $(AUX_OBJS) $(PROJ_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(AUX_OBJS) $(PROJ_OBJS) $(LDFLAGS)

.PHONY: notabs
notabs:
	find . -type f -regex ".*\.[ch]" -exec sed -i -e "s/\t/    /g" {} +

.PHONY: clean
clean:
	rm -f core *.gcno *.gcda coverage*html coverage.css *.log \
	$(TEST_OBJS) $(TEST_BINS) $(AUX_OBJS) \
	$(PROJ_OBJS) $(STATIC_LIB) $(SHARED_LIB) $(SHARED_LINK)
