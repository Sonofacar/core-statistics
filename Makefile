PREFIX = /usr/local

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
profile: CFLAGS += -pg
profile: clean all

# Coverage for use with gcov
coverage: CFLAGS += -fprofile-arcs -ftest-coverage
coverage: clean all

# Build and run tests
test: $(TEST_BIN)
	./$(TEST_BIN)

# Link test binary
$(TEST_BIN): $(TEST_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(CMOCKA_LDFLAGS)

lm: lm.c core.h
	gcc $(CFLAGS) lm.c -o $@ $(GSL_CFLAGS) $(GSL_LDFLAGS)

clean:
	rm -f lm gmon.out *.gcov *.gcno $(TEST_OBJS) $(TEST_BIN)

install: lm
	$(MAKE) clean
	$(MAKE) all
	strip lm
	install -m 755 lm $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/lm

.PHONY: clean install uninstall
