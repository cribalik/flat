INCLUDES := -Iinclude
FLAGS := -ansi -pedantic -g -Wall -Wextra -Wno-unused-function -DDEBUG=1
LD_LIBRARY_PATH := 3party

all:
	mkdir -p build
	gcc $(FLAGS) $(INCLUDES) src/flatsouls.c -fPIC -shared -o build/flatsouls.so
	gcc $(FLAGS) $(INCLUDES) src/flatsouls_linux.c -o build/flatsouls -L3party -lSDL2 -ldl -lGL

clean:
	rm -r build/*
