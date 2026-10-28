SRC = ../../main
CFLAGS = -O2 -Wall -Wextra -I$(SRC) -I../../../../shared/nibbles_link/include

test: build/test
	./build/test

build/test: test_main.c $(SRC)/controls.c $(SRC)/power_math.c $(wildcard $(SRC)/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ test_main.c $(SRC)/controls.c $(SRC)/power_math.c -lm

.PHONY: test
