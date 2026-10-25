CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Isrc
LDLIBS  := -lm

SRC := src/world.c src/player.c src/render.c src/game.c
OBJ := $(SRC:.c=.o)

game3d: $(OBJ) src/main.o
	$(CC) $(CFLAGS) -o $@ $(OBJ) src/main.o $(LDLIBS)

test/test_game3d: $(OBJ) test/test_game3d.o
	$(CC) $(CFLAGS) -o $@ $(OBJ) test/test_game3d.o $(LDLIBS)

test: test/test_game3d
	./test/test_game3d

demo: game3d
	./game3d 7 frame

clean:
	rm -f $(OBJ) src/main.o test/test_game3d.o game3d test/test_game3d frame_*.ppm

.PHONY: test demo clean
