SRC = ../../main
CFLAGS = -O2 -Wall -Wextra -I$(SRC)

test: build/test
	./build/test

build/test: test_main.c $(SRC)/audio_analysis.c $(SRC)/motion_analysis.c $(SRC)/eye.c $(SRC)/presets.c $(wildcard $(SRC)/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ test_main.c $(SRC)/audio_analysis.c $(SRC)/motion_analysis.c $(SRC)/eye.c $(SRC)/presets.c -lm

.PHONY: test
