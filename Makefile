PREFIX = /usr/local

COVERAGE_FLAGS = -fprofile-arcs -ftest-coverage
PROFILE_FLAGS = -pg

TEST_DIR = tests
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_BIN = runtests

CMOCKA_CFLAGS = $(shell pkg-config --cflags cmocka)
CMOCKA_LDFLAGS = $(shell pkg-config --libs cmocka)

GSL_CFLAGS = $(shell gsl-config --cflags)
GSL_LDFLAGS = $(shell gsl-config --libs)

all: lm

# Profiling for use with gprof
profile: CFLAGS += $(COVERAGE_FLAGS)
profile: clean all

# Coverage for use with gcov
coverage: CFLAGS += $(PROFILE_FLAGS)
coverage: clean all

# Build and run tests
test: CFLAGS += $(COVERAGE_FLAGS)
test: $(TEST_BIN)
	./$(TEST_BIN)
	@echo "Coverage report:"
	@gcov -o $(TEST_DIR) $(TEST_BIN)

# Link test binary
$(TEST_BIN): $(TEST_OBJS)
	gcc $(CFLAGS) $(GSL_CFLAGS) $^ -o $@ $(GSL_LDFLAGS) $(CMOCKA_LDFLAGS)

lm: lm.c core.h
	gcc $(CFLAGS) $(GSL_CFLAGS) lm.c -o $@ $(GSL_LDFLAGS)

clean:
	rm -f lm gmon.out *.gcov *.gcno *.gcda $(TEST_OBJS) $(TEST_BIN) $(TEST_DIR)/*.gcda $(TEST_DIR)/*.gcno

install: lm
	$(MAKE) clean
	$(MAKE) all
	strip lm
	install -m 755 lm $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/lm

.PHONY: all profile coverage clean test install uninstall
