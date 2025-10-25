CC := gcc
CFLAGS := -Wall -Wextra -std=gnu11 -g $(shell gsl-config --cflags)
LDFLAGS := $(shell gsl-config --libs)
TEST_LIBS := $(shell pkg-config --libs cmocka)
SRC := core.c encode.c lm.c
OBJS := $(SRC:.c=.o)
TARGET := lm

TEST_SRC := runtests.c core.c encode.c
TEST_OBJS := $(TEST_SRC:.c=.o)
TEST_BIN := runtests

# List wrapped functions here (no need for quotes or commas)
WRAPS := malloc gsl_vector_alloc free

# Convert WRAPS list into linker flags
WRAP_FLAGS := $(foreach f,$(WRAPS),-Wl,--wrap=$(f))

.PHONY: all clean test

# Default target
all: $(TARGET)

# Main program
$(TARGET): $(OBJS)
	$(CC) -static $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Run tests
test: $(TEST_BIN)
	@echo "Running tests..."
	@./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(TEST_LIBS) $(WRAP_FLAGS)

# Generic compile rule
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) $(TEST_BIN)
