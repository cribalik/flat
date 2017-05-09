INCLUDES := -Iinclude
FLAGS := -g -Wall -Wextra -Werror -DDEBUG=1
LD_LIBRARY_PATH := 3party
LFLAGS := -L3party -lSDL2

all:
	gcc $(FLAGS) $(INCLUDES) src/flatsouls.c -o flatsouls.out $(LFLAGS)

clean:
	rm flatsouls.out
