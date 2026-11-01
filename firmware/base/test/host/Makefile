SRC = ../../main
CFLAGS = -O2 -Wall -Wextra -I$(SRC) -I../../../../shared/nibbles_link/include

test: build/test
	./build/test

build/test: test_main.c $(SRC)/controls.c $(SRC)/power_math.c $(SRC)/log_format.c $(SRC)/power_sim.c $(SRC)/eye_thumb.c ../../../eyes/main/presets.c $(wildcard $(SRC)/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ test_main.c $(SRC)/controls.c $(SRC)/power_math.c $(SRC)/log_format.c $(SRC)/power_sim.c $(SRC)/eye_thumb.c ../../../eyes/main/presets.c -lm

.PHONY: test
