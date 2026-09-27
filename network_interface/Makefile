TARGET = db_listener
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, obj/%.o, $(SRC))

default: $(TARGET)

clean:
	rm obj/*.*
	rm *log*
	rm *.lock
	rm $(TARGET)

$(TARGET): $(OBJ)
	gcc -o $@ $? -ldblua -llua5.4 -lfree -llog -lmem -lcrud -z relro -z now -z noexecstack -fsanitize=address 


obj/%.o : src/%.c
	gcc -Wall -Wextra -Walloca -Warray-bounds -Wnull-dereference -g3 -c $< -o $@ -Iinclude -I../lua/include -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIC -fsanitize=address 
