CFLAGS = -O2 -Wall -Wextra -Iinclude

test: build/test
	./build/test

build/test: test/test_link.c src/nl_frame.c src/nl_chanscan.c src/nl_bump.c src/nl_ar.c src/nl_power.c src/nl_stream.c include/nibbles_link.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ test/test_link.c src/nl_frame.c src/nl_chanscan.c src/nl_bump.c src/nl_ar.c src/nl_power.c src/nl_stream.c -lm

.PHONY: test
