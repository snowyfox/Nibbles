SRC = ../../main
CFLAGS = -O2 -Wall -Wextra -I$(SRC) -I../../../../shared/nibbles_link/include

test: build/test
	./build/test

build/test: test_main.c $(SRC)/audio_analysis.c $(SRC)/motion_analysis.c $(SRC)/eye.c $(SRC)/presets.c $(wildcard $(SRC)/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ test_main.c $(SRC)/audio_analysis.c $(SRC)/motion_analysis.c $(SRC)/eye.c $(SRC)/presets.c -lm

.PHONY: test

# Contact sheet of the cartoon animations (not a test): build/cartoon_preview OUT.ppm
cartoon: build/cartoon_preview

build/cartoon_preview: cartoon_preview.c $(SRC)/cartoon.c $(SRC)/cartoon.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ cartoon_preview.c $(SRC)/cartoon.c -lm

.PHONY: cartoon
