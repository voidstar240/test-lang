.PHONY = all db clean

EXEC := lang
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src%,obj%, $(patsubst %.c,%.o, $(SRCS)))

CFLAGS := -Wall -pedantic -Werror=incompatible-pointer-types-discards-qualifiers -std=c99 -g -O0
CC := clang

all: $(EXEC) db

# Link object files to create executable
$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC)

# Compile all *.c files to *.o
obj/%.o: src/%.c
	@mkdir -p $(@D) # Make `obj/` dir
	$(CC) $(CFLAGS) -c $< -o $@

db:
	compiledb -nf make

clean:
	rm -rf obj/
	rm -f $(EXEC)
	rm -rf .cache/
	rm -f compile_commands.json
