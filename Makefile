# Project Setup
PROJECT := librayco
STATIC_LIB  := $(PROJECT).a
SHARED_LIB  := $(PROJECT).so.0
SHARED_LINK := $(PROJECT).so
SRCDIR   := ./src
OBJDIR   := ./obj

# Gather project sources and folders, start cflags early
SOURCES := $(notdir $(shell find ./src -follow -name '*.c'))
FOLDERS := $(sort $(dir $(shell find ./src -follow -name '*.c')))
CFLAGS  := -Wall -pipe -fPIC

# Linenoise is an optional third-party submodule
# If found, the shell object will have tab-completion and argument hints
LINENOISE_DIR = ./ext/linenoise
LINENOISE_FILE = $(LINENOISE_DIR)/linenoise.c
ifneq ("$(wildcard $(LINENOISE_FILE))", "")
	CFLAGS  += -D LINENOISE_ENABLE
	SOURCES += $(LINENOISE_DIR)/linenoise.c
	FOLDERS += $(LINENOISE_DIR)
endif

# Create object paths and compiler include arguments
OBJECTS  := $(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(SOURCES)))
INCLUDE  := $(patsubst %,-I%,$(FOLDERS))
VPATH    := $(FOLDERS)

# Unit Test Configuration
TEST_SRCS := $(notdir $(shell find ./test -follow -name 'test_*.c'))
TEST_DIRS := $(sort $(dir $(shell find ./test -follow -name 'test_*.c')))
TEST_OBJS := $(patsubst %.c,%.o,$(TEST_SRCS))
TEST_BINS := $(patsubst %.c,%.mut,$(TEST_SRCS))
TEST_INCL := $(patsubst %,-I%,$(TEST_DIRS))
AUX_SRCS  := $(notdir $(shell find ./test -follow -name '*.c' -not -name 'test*'))
AUX_OBJS  := $(patsubst %.c,%.o,$(AUX_SRCS))
VPATH     += $(TEST_DIRS)

# Toolchain Configuration
AR           := ar
LD           := ld
CC           := gcc
BIN          := /usr/local/bin
LIB          := /usr/local/lib
CFLAGS       += $(INCLUDE)
DEBUG_CFLAGS := -O0 -g -D BLAMMO_ENABLE -fmax-errors=3

# Platform Conditional Linker Flags
ifeq ($(ANDROID_ROOT),)
	LDFLAGS      := -lc -pie
	COV_REPORT   := gcovr -r . --html-details -o coverage.html 
else
	LDFLAGS      := -pie
	COV_REPORT   :=
endif

# Debug Info
$(info $$INCLUDE is [${INCLUDE}])
$(info $$SOURCES is [${SOURCES}])
$(info $$FOLDERS is [${FOLDERS}])
$(info $$OBJECTS is [${OBJECTS}])
$(info $$CFLAGS is [${CFLAGS}])

# Make Targets
.PHONY: all
all: CFLAGS += -O2 -fomit-frame-pointer
all: $(STATIC_LIB) $(SHARED_LIB)

.PHONY: debug
debug: CFLAGS += $(DEBUG_CFLAGS)
debug: $(STATIC_LIB) $(SHARED_LIB)

# Pattern match rule for project sources
$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Pattern match rule(s) for third party submodule sources
$(OBJDIR)/%.o: $(LINENOISE_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(STATIC_LIB): $(OBJECTS)
	$(AR) rcs $(STATIC_LIB) $(OBJECTS)

$(SHARED_LIB): $(OBJECTS)
	#$(LD) $(LDFLAGS) -shared -soname,$(SHARED_LIB) -o $(SHARED_LIB) $(OBJECTS)
	$(LD) $(LDFLAGS) -shared -o $(SHARED_LIB) $(OBJECTS)

.PHONY: test
test: CFLAGS += $(TEST_INCL) $(DEBUG_CFLAGS) -Wno-unused-label
ifeq ($(ANDROID_ROOT),)
test: CFLAGS += -fprofile-arcs -ftest-coverage
endif
test: $(TEST_BINS)
	for testmut in test_*mut; do ./$$testmut; done
	$(COV_REPORT)

test_%.mut : test_%.o $(AUX_OBJS) $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $< $(AUX_OBJS) $(OBJECTS) $(LDFLAGS)

.PHONY: notabs
notabs:
	find . -type f -regex ".*\.[ch]" -exec sed -i -e "s/\t/    /g" {} +

.PHONY: clean
clean:
	rm -f core *.gcno *.gcda coverage*html coverage.css *.log \
	$(TEST_OBJS) $(TEST_BINS) $(AUX_OBJS) $(OBJDIR)/* \
	$(OBJECTS) $(STATIC_LIB) $(SHARED_LIB) $(SHARED_LINK)
