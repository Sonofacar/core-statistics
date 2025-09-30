PREFIX = /usr/local

lm: lm.c core.h
	gcc lm.c -o $@ -ggdb $$(gsl-config --cflags)  $$(gsl-config --libs)

clean:
	rm -f lm

install: lm
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f lm $(DESTDIR)$(PREFIX)/bin
	chmod 775 $(DESTDIR)$(PREFIX)/bin/lm

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/lm

.PHONY: clean install uninstall
