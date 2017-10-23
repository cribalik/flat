INCLUDES := -Iinclude
FLAGS := -ansi -pedantic -g -Wall -Wextra -Wno-unused-function -DDEBUG=1

all:
	mkdir -p build
	gcc $(FLAGS) $(INCLUDES) src/flat.c -fPIC -shared -o build/flat.so -lm
	gcc $(FLAGS) $(INCLUDES) src/flat_linux.c -o build/flat -Llib -lSDL2 -ldl -lGL -lSOIL -lm

clean:
	rm -r build/*
