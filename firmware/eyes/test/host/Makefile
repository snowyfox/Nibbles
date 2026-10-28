SRC = ../../main
CFLAGS = -O2 -Wall -Wextra -I$(SRC) -I../../../../shared/nibbles_link/include

test: build/test
	./build/test

build/test: test_main.c $(SRC)/audio_analysis.c $(SRC)/motion_analysis.c $(SRC)/eye.c $(SRC)/presets.c $(wildcard $(SRC)/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ test_main.c $(SRC)/audio_analysis.c $(SRC)/motion_analysis.c $(SRC)/eye.c $(SRC)/presets.c -lm

.PHONY: test

# The built-in presets as eye looks in WLED preset JSON (wled/README.md, "Eye settings in presets").
looks: build/export_looks
	./build/export_looks

build/export_looks: export_looks.c $(SRC)/presets.c $(SRC)/presets.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ export_looks.c $(SRC)/presets.c -lm

.PHONY: looks
