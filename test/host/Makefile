SRC = ../../main
CFLAGS = -O2 -Wall -Wextra -I$(SRC)

test: build/test
	./build/test

build/test: test_main.c $(SRC)/audio_analysis.c $(SRC)/motion_analysis.c $(SRC)/eye.c $(wildcard $(SRC)/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ test_main.c $(SRC)/audio_analysis.c $(SRC)/motion_analysis.c $(SRC)/eye.c -lm

.PHONY: test
