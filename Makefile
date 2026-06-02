CC := gcc
CFLAGS := -Wall -Wextra -std=gnu11 -g -Iinclude $(shell gsl-config --cflags)
LDFLAGS := $(shell gsl-config --libs)
TEST_LIBS := $(shell pkg-config --libs cmocka)
COMMON_SRC := src/core.c \
	      src/encode.c \
	      src/model_utils.c
COMMON_OBJS := $(COMMON_SRC:src/%.c=build/%.o)
TARGETS := lm tsvdlm

TEST_SRC := src/runtests.c
TEST_OBJS := $(TEST_SRC:src/%.c=build/%.o)
TEST_BIN := runtests

# List wrapped functions here (no need for quotes or commas)
WRAPS := malloc gsl_vector_alloc free

# Convert WRAPS list into linker flags
WRAP_FLAGS := $(foreach f,$(WRAPS),-Wl,--wrap=$(f))

.PHONY: all clean test

# Default target
all: $(TARGETS)

# Main program
define MAKE_PROGRAM
$1: build/$1.o $(COMMON_OBJS)
	$$(CC) $$(CFLAGS) -o $$@ $$^ $$(LDFLAGS)
endef
$(foreach target,$(TARGETS),$(eval $(call MAKE_PROGRAM,$(target))))
	
# Run tests
test: $(TEST_BIN)
	@echo "Running tests..."
	@./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(TEST_LIBS) $(WRAP_FLAGS)

# Generic compile rule
build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f build/* $(TARGETS)
