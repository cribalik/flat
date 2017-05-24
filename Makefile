INCLUDES := -Iinclude
FLAGS := -ansi -pedantic -g -Wall -Wextra -Wno-unused-function -DDEBUG=1

all:
	mkdir -p build
	gcc $(FLAGS) $(INCLUDES) src/flatsouls.c -fPIC -shared -o build/flatsouls.so -lm
	gcc $(FLAGS) $(INCLUDES) src/flatsouls_linux.c -o build/flatsouls -Llib -lSDL2 -ldl -lGL -lSOIL -lm

clean:
	rm -r build/*
