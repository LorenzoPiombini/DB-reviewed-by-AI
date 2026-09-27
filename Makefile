TARGET = /usr/local/bin/isam.db
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, obj/%.o, $(SRC))
OBJ_PROD = $(patsubst src/%.c, obj/%_prod.o, $(SRC))

OBJlibdate = obj/date.o 
OBJlibht = obj/debug.o  obj/hash_tbl.o
OBJlibhtPR = obj/debug_prod.o  obj/hash_tbl_prod.o 
OBJlibf = obj/debug.o  obj/file.o  obj/endian.o 
OBJlibfPR = obj/debug_prod.o  obj/file_prod.o  obj/endian_prod.o 
OBJlibs = obj/debug.o  obj/str_op.o
OBJlibsPR = obj/debug_prod.o  obj/str_op_prod.o
OBJlibr = obj/debug.o  obj/record.o obj/parse.o
OBJlibrPR = obj/debug_prod.o  obj/record_prod.o
OBJlibp = obj/debug.o  obj/sort.o obj/parse.o
OBJlibpPR = obj/debug_prod.o  obj/sort_prod.o obj/parse_prod.o
OBJlibl = obj/debug.o  obj/lock.o
OBJliblPR = obj/debug_prod.o  obj/lock_prod.o
OBJlibcrud = obj/crud.o obj/file.o obj/date.o obj/hash_tbl.o obj/debug.o obj/common.o obj/string_utilities.o obj/str_op.o obj/lock.o obj/record.o obj/endian.o obj/parse.o obj/globals.o obj/sort.o obj/input.o obj/key.o

OBJlibexpl = obj/export_db_lua.o 
OBJlibexplPR = obj/export_db_lua_prod.o 

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

LIBNAMEexpl = dblua
LIBDIR = /usr/local/lib
INCLUDEDIR = /usr/local/include
SHAREDLIBexpl = lib$(LIBNAMEexpl).so

LIBNAMEht = ht
LIBDIR = /usr/local/lib
INCLUDEDIR = /usr/local/include
SHAREDLIBht = lib$(LIBNAMEht).so

LIBNAMEdate = date
LIBDIR = /usr/local/lib
INCLUDEDIR = /usr/local/include
SHAREDLIBdate = lib$(LIBNAMEdate).so

LIBNAMEcrud = crud
LIBDIR = /usr/local/lib
INCLUDEDIR = /usr/local/include
SHAREDLIBcrud = lib$(LIBNAMEcrud).so

LIBNAMEf = file
LIBDIR = /usr/local/lib
INCLUDEDIR = /usr/local/include
SHAREDLIBf = lib$(LIBNAMEf).so

LIBNAMEs = strOP
LIBDIR = /usr/local/lib
INCLUDEDIR = /usr/local/include
SHAREDLIBs = lib$(LIBNAMEs).so

LIBNAMEr = record
LIBDIR = /usr/local/lib
INCLUDEDIR = /usr/local/include
SHAREDLIBr = lib$(LIBNAMEr).so

LIBNAMEp = parse
LIBDIR = /usr/local/lib
INCLUDEDIR = /usr/local/include
SHAREDLIBp = lib$(LIBNAMEp).so

LIBNAMEl = lock
LIBDIR = /usr/local/lib
INCLUDEDIR = /usr/local/include
SHAREDLIBl = lib$(LIBNAMEl).so

# Detect if the OS is Fedora
IS_FEDORA := $(shell grep -E '^ID="?fedora"?' /etc/os-release > /dev/null 2>&1 && echo yes || echo no)

default: $(TARGET)


prod: $(TARGET)_prod

object-dir:
	@if [ ! -d ./obj ]; then\
		echo "creating object directory...";\
		mkdir -p obj ;\
	fi

check-linker-path:
	@if [ ! -f /etc/ld.so.conf.d/customtech.conf ]; then \
		echo "setting linker configuration..." ;\
		echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/customtech.conf >/dev/null ;\
		sudo ldconfig;\
	fi

library:
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBht) $(OBJlibht)
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBcrud) $(OBJlibcrud)
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBs) $(OBJlibs)
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBr) $(OBJlibr)
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBf) $(OBJlibf)
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBp) $(OBJlibp)
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBl) $(OBJlibl)
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBdate) $(OBJlibdate)


libraryPR:
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBht) $(OBJlibhtPR)
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBs) $(OBJlibsPR)
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBr) $(OBJlibrPR)
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBf) $(OBJlibfPR)
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBp) $(OBJlibpPR)
	sudo gcc -Wall -fPIC -shared -o $(SHAREDLIBl) $(OBJliblPR)

clean:
	sudo rm -f $(BINDIR)/CHANGE_NAMES $(BINDIR)/$(TARGET) $(BINDIR)/SWAP_INDEXES $(BINDIR)/GET $(BINDIR)/LIST $(BINDIR)/FILE $(BINDIR)/KEYS $(BINDIR)/WRITE $(BINDIR)/UPDATE $(BINDIR)/DEL $(BINDIR)/DELa
	sudo rm -f $(INCLUDEDIR)/file.h $(INCLUDEDIR)/str_op.h $(INCLUDEDIR)/record.h $(INCLUDEDIR)/parse.h $(INCLUDEDIR)/bst.h $(INCLUDEDIR)/hash_tbl.h $(INCLUDEDIR)/lock.h 
	sudo rm -f $(LIBDIR)/$(SHAREDLIBf) $(LIBDIR)/$(SHAREDLIBs) $(LIBDIR)/$(SHAREDLIBr) $(LIBDIR)/$(SHAREDLIBp) $(LIBDIR)/$(SHAREDLIBht) $(LIBDIR)/$(SHAREDLIBl)
	sudo ldconfig
	rm -f obj/*.o 
	sudo rm -f *.so
	sudo rm -f $(TARGET)
	rm -f *.lock
	rm *core*
	#rm *.dat *.inx *.sch
	 

test:
	if [ "$(IS_FEDORA)" = "no" ]; then \
		gcc -g3 -o test/test_suite test/src/main.c test/src/test.c network_interface/src/lua_start.c -lcrud -llua5.4 -fsanitize=address -Itest/include;\
	else\
		gcc -g3 -DFEDORA -o  test/test_suite test/src/main.c test/src/test.c network_interface/src/lua_start.c -lcrud -llua -fsanitize=address -Itest/include;\
	fi
	test/test_suite
	
$(TARGET): $(OBJ)
	sudo gcc -o $@ $?  -ldl  -fpie -pie -z relro -z now -z noexecstack -fsanitize=address 
	make lua

obj/%.o : src/%.c 
	if [ "$(IS_FEDORA)" = "no" ]; then \
		sudo gcc  -std=c89 -Werror -Wall -Wextra -Walloca -Warray-bounds -Wnull-dereference -g3 -c $< -o $@ -Iinclude -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIC -pie -fsanitize=address;\
	else\
		sudo gcc  -std=c89 -Werror -Wall -Wextra -Walloca -Warray-bounds -Wnull-dereference -g3 -c $< -o $@ -Iinclude -DFEDORA -fstack-protector-strong -fPIC -pie -fsanitize=address;\
	fi

lua: lua_obj
	if [ "$(IS_FEDORA)" = "no" ]; then \
		gcc -shared -o db.so obj/export_db_lua.o  -L/usr/local/lib -lcrud -llua5.4  -ldl -fsanitize=address;\
		mv db.so /usr/local/lib/lua/5.4/; \
	else \
		gcc -shared -o db.so obj/export_db_lua.o  -L/usr/local/lib -lcrud -llua -DFEDORA -ldl -fsanitize=address;\
		cp db.so /usr/share/lua/5.4/;\
		cp db.so /usr/lib64/lua/5.4/;\
	fi

lua_obj: 
	if [ "$(IS_FEDORA)" = "no" ]; then \
		sudo gcc -g3 -fPIC -Wall -c lua/src/export_db_lua.c  -lcrud  -Iinclude -Ilua/include -I/usr/include/lua5.4  -o obj/export_db_lua.o;\
	else \
		sudo gcc -g3 -fPIC -Wall -c lua/src/export_db_lua.c  -lcrud  -DFEDORA -Iinclude -Ilua/include -I/usr/include/lua -o obj/export_db_lua.o -fsanitize=address;\
	fi

net_int:
	if [ "$(IS_FEDORA)" = "no" ]; then \
		sudo cp network_interface/include/lua_start.h network_interface/include/end_points.h network_interface/worker_process.h ;\
		sudo gcc -Wall -Wextra -Walloca -Warray-bounds -Wnull-dereference -g3 -c network_interface/src/lua_start.c -o network_interface/obj/lua_start.o -Iinclude -I/usr/local/include/ -Inetwork_interface/include -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIC -pie -fsanitize=address;\
		sudo gcc -Wall -Wextra -Walloca -Warray-bounds -Wnull-dereference -g3 -c network_interface/src/end_points.c -o network_interface/obj/end_points.o -Iinclude -I/usr/local/include/ -Inetwork_interface/include -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIC -pie -fsanitize=address;\
		sudo gcc -Wall -Wextra -Walloca -Warray-bounds -Wnull-dereference -g3 -c network_interface/src/worker_process.c -o network_interface/obj/worker_process.o -Iinclude -I/usr/local/include/ -Inetwork_interface/include -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIC -pie -fsanitize=address;\
		gcc -shared  network_interface/obj/lua_start.o network_interface/obj/worker_process.o network_interface/obj/end_points.o -o libworker.so -fPIC -llua5.4 -lcrud ;\
		cp libworker.so /usr/local/lib/;\
		cp network_interface/lua/db_config.lua /root/db/lua/;\
	else \
		sudo cp network_interface/include/lua_start.h network_interface/include/end_points.h network_interface/include/worker_process.h ;\
		sudo gcc -DFEDORA -Wall -Wextra -Walloca -Warray-bounds -Wnull-dereference -g3 -c network_interface/src/lua_start.c -o network_interface/obj/lua_start.o -Iinclude -I/usr/local/include/ -Inetwork_interface/include -fstack-protector-strong -fPIC -pie -fsanitize=address;\
		sudo gcc -Wall -DFEDORA -Wextra -Walloca -Warray-bounds -Wnull-dereference -g3 -c network_interface/src/end_points.c -o network_interface/obj/end_points.o -Iinclude -I/usr/local/include/ -Inetwork_interface/include -fstack-protector-strong  -fPIC -pie -fsanitize=address;\
		sudo gcc -Wall -DFEDORA -Wextra -Walloca -Warray-bounds -Wnull-dereference -g3 -c network_interface/src/worker_process.c -o network_interface/obj/worker_process.o -Iinclude -I/usr/local/include/ -Inetwork_interface/include -fstack-protector-strong -fPIC -pie -fsanitize=address;\
		gcc -DFEDORA -shared  network_interface/obj/lua_start.o network_interface/obj/worker_process.o network_interface/obj/end_points.o -o libworker.so -fPIC -llua -lcrud -fsanitize=address;\
		cp libworker.so /usr/lib64/;\
		cp libworker.so /usr/local/lib/;\
		cp network_interface/lua/db_config.lua /root/db/lua/;\
	fi

$(TARGET)_prod: $(OBJ_PROD)
	sudo gcc -o $@ $? -fpie -pie -z relro -z now -z noexecstack

obj/%_prod.o : src/%.c
	sudo gcc -Wall -g3 -c $< -o $@ -DPROD -Iinclude -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIC



$(BINDIR)/SWAP_INDEXES:
	@if [ !  -f $@ ]; then \
		echo "Creating $@ . . ."; \
		echo "#!/bin/bash" > $@; \
		echo "#Check if both arguments are provided" >> $@; \
		echo "if [ -z \"\$$1\" ] || [ -z \"\$$2\" ] || [ -z \"\$$3\" ]; then" >> $@; \
		echo "echo \"Usage: SWAP_INDEXES [file name] [source index] [dest index]\"" >> $@; \
		echo "exit 1" >> $@; \
		echo "fi" >> $@; \
		echo "" >> $@; \
		echo "$(TARGET) -Sf \"\$$1\" -s \"\$$2\" -i \"\$$3\"">> $@; \
		chmod +x $@; \
	fi

$(BINDIR)/SHOW:
	@if [ !  -f $@ ]; then \
		echo "Creating $@ . . ."; \
		echo "#!/bin/bash" > $@; \
		echo "#Check if both arguments are provided" >> $@; \
		echo "if [ -z \"\$$1\" ] || [ -z \"\$$2\" ]; then" >> $@; \
		echo "echo \"Usage: SHOW [file name] [record_id]\"" >> $@; \
		echo "exit 1" >> $@; \
		echo "fi" >> $@; \
		echo "" >> $@; \
		echo "value=\"\$$3\"" >> $@; \
		echo "if [ -z \"\$$3\" ]; then" >> $@; \
		echo "value=0" >> $@; \
		echo "fi" >> $@; \
		echo "" >> $@; \
		echo "$(TARGET) -f \"\$$1\" -k \"\$$2\" -X \"\$$value\"">> $@; \
		chmod +x $@; \
	fi
$(BINDIR)/LIST:
	@if [ !  -f $@ ]; then  \
		echo "Creating $@ . . ."; \
		echo "#!/bin/bash" > $@; \
		echo "#Check if the argument is provided" >> $@; \
		echo "if [ -z \"\$$1\" ]; then" >> $@; \
		echo "echo \"Usage: LIST [file name]\"" >> $@; \
		echo "exit 1" >> $@; \
		echo "fi" >> $@; \
		echo "" >> $@; \
		echo "$(TARGET) -lf \"\$$1\"" >> $@; \
		chmod +x $@; \
	fi

$(BINDIR)/FILE:
	@if [ ! -f $@ ]; then \
		echo "Creating $@ . . ."; \
		echo "#!/bin/bash" > $@; \
		echo "if [ -z \"\$$1\" ] || [ -z \"\$$2\" ]; then" >> $@; \
		echo "echo \"Usage: FILE [file name] [fields name and type]\"" >> $@; \
		echo "exit 1" >> $@; \
		echo "fi" >> $@; \
		echo "" >> $@; \
		echo "" >> $@; \
		echo "if [ -e \"\$$1.dat\" ]; then" >> $@; \
		echo "	 $(TARGET) -f \"\$$1\" -R \"\$$2\"" >> $@; \
		echo "else" >> $@; \
		echo "$(TARGET) -nf \"\$$1\" -R \"\$$2\"" >> $@; \
		echo "fi" >> $@; \
		chmod +x $@; \
	fi

$(BINDIR)/WRITE:
	@if [ ! -f $@ ]; then \
		echo "Creating $@ . . ."; \
		echo "#!/bin/bash" > $@; \
		echo "if [ -z \"\$$1\" ] || [ -z \"\$$2\" ]; then" >> $@; \
		echo "echo \"Usage: WRITE [file name] [fields name and type] (optional: [key])\"" >> $@; \
		echo "exit 1" >> $@; \
		echo "fi" >> $@; \
		echo "if [ -n \"\$$3\" ]; then" >> $@; \
		echo "\t$(TARGET) -f \"\$$1\" -a \"\$$2\" -k \"\$$3\" " >> $@; \
		echo "exit 0" >> $@; \
		echo "fi" >> $@; \
		echo "$(TARGET) -f \"\$$1\" -a \"\$$2\"" >> $@; \
		chmod +x $@; \
	fi

$(BINDIR)/UPDATE:
	@if [ ! -f $@ ]; then \
		echo "Creating $@ . . ."; \
		echo "#!/bin/bash" > $@; \
		echo "if [ -z \"\$$1\" ] || [ -z \"\$$2\" ] || [ -z \"\$$3\" ]; then" >> $@; \
		echo "echo \"Usage: UPDATE [file name] [fields name and type] [key]\"" >> $@; \
		echo "exit 1" >> $@; \
		echo "fi" >> $@; \
		echo "if [ -n \"\$$4\" ] && [ -n \"\$$5\" ]; then" >> $@; \
		echo "	$(TARGET) -uf \"\$$1\" -a \"\$$2\" -k \"\$$3\" -o \"\$$4\"  -x \"\$$5\" " >> $@; \
		echo "	exit 0" >> $@; \
		echo "fi" >> $@; \
		echo "if [ -n \"\$$4\" ]; then" >> $@; \
		echo "	if [[ \"\$$4\" =~ ^[0-9]+\$$ ]]; then" >> $@; \
		echo "		$(TARGET) -uf \"\$$1\" -a \"\$$2\" -k \"\$$3\" -x \"\$$4\" " >> $@; \
		echo " 	else" >> $@; \
		echo "		$(TARGET) -uf \"\$$1\" -a \"\$$2\" -k \"\$$3\" -o \"\$$4\" " >> $@; \
		echo "	fi" >> $@; \
		echo "	exit 0" >> $@; \
		echo "fi" >> $@; \
		echo "$(TARGET) -uf \"\$$1\" -a \"\$$2\" -k \"\$$3\"" >> $@; \
		chmod +x $@; \
	fi
$(BINDIR)/KEYS:
	@if [ ! -f $@ ]; then \
		echo "Creating $@ . . ."; \
		echo "#!/bin/bash" > $@; \
		echo "if [ -z \"\$$1\" ]; then" >> $@; \
		echo "echo \"Usage: KEYS [file name]\"" >> $@; \
		echo "exit 1" >> $@; \
		echo "fi" >> $@; \
		echo "" >> $@; \
		echo "value=\"\$$2\"" >> $@; \
		echo "if [ -z \"\$$2\" ]; then" >> $@; \
		echo "value=0" >> $@; \
		echo "fi" >> $@; \
		echo "$(TARGET) -f \"\$$1\" -x \"\$$value\"" >> $@; \
		chmod +x $@; \
	fi
$(BINDIR)/DEL:
	@if [ ! -f $@ ]; then \
		echo "Creating $@ . . ."; \
		echo "#!/bin/bash" > $@; \
		echo "if [ -z \"\$$1\" ] || [ -z \"\$$2\" ]; then" >> $@; \
		echo "echo \"Usage: DEL [file name] [key] [index_number]\"" >> $@; \
		echo "echo \"index number is not mandatory\"" >> $@; \
		echo "echo \"if index_number is not specified index 0 is used\"" >> $@; \
		echo "exit 1" >> $@; \
		echo "fi" >> $@; \
		echo "" >> $@; \
		echo "value=\"\$$3\"" >> $@; \
		echo "if [ -z \"\$$3\" ]; then" >> $@; \
		echo "value=0" >> $@; \
		echo "fi" >> $@; \
		echo "" >> $@; \
		echo "$(TARGET) -f \"\$$1\" -k \"\$$2\" -D \"\$$value\"" >> $@; \
		chmod +x $@; \
	fi

$(BINDIR)/DELa:
	@if [ ! -f $@ ]; then \
		echo "Creating $@ . . ."; \
		echo "#!/bin/bash" > $@; \
		echo "if [ -z \"\$$1\" ]; then" >> $@; \
		echo "echo \"Usage: DELa [file name]\"" >> $@; \
   		echo "exit 1" >> $@; \
   		echo "fi" >> $@; \
		echo "$(TARGET) -f \"\$$1\" -D 0 -o all" >> $@; \
		chmod +x $@; \
	fi

$(BINDIR)/CHANGE_NAMES:
	@if [ ! -f $@ ]; then \
		echo "Creating $@ . . ."; \
		echo "#!/bin/bash" > $@; \
		echo "if [ -z \"\$$1\" ] || [ -z \"\$$2\" ]; then" >> $@; \
		echo "echo \"Usage: CHANGE_NAMES [file name] [fields:to:change]\"" >> $@; \
   		echo "exit 1" >> $@; \
   		echo "fi" >> $@; \
		echo "$(TARGET) -Mf \"\$$1\" -R \"\$$2\"" >> $@; \
		chmod +x $@; \
	fi

install: $(TARGET) $(BINDIR)/SWAP_INDEXES $(BINDIR)/SHOW $(BINDIR)/LIST $(BINDIR)/FILE $(BINDIR)/KEYS $(BINDIR)/WRITE $(BINDIR)/UPDATE $(BINDIR)/DEL $(BINDIR)/DELa $(BINDIR)/CHANGE_NAMES check-linker-path
	install -d $(INCLUDEDIR)
	install -m 644 include/date.h lua/include/export_db_lua.h include/globals.h include/hash_tbl.h include/file.h include/key.h include/str_op.h include/record.h include/common.h include/types.h include/parse.h include/lock.h include/crud.h include/string_utilities.h network_interface/include/lua_start.h network_interface/include/worker_process.h network_interface/include/end_points.h $(INCLUDEDIR)/
	install -m 755 $(SHAREDLIBht) $(LIBDIR)
	install -m 755 $(SHAREDLIBcrud) $(LIBDIR)
	install -m 755 $(SHAREDLIBf) $(LIBDIR)
	install -m 755 $(SHAREDLIBs) $(LIBDIR)
	install -m 755 $(SHAREDLIBr) $(LIBDIR)
	install -m 755 $(SHAREDLIBp) $(LIBDIR) 
	install -m 755 $(SHAREDLIBl) $(LIBDIR)
	install -m 755 $(SHAREDLIBdate) $(LIBDIR)
	ldconfig
	make net_int
	make test
	

install_prod: $(TARGET)_prod $(BINDIR)/SHOW $(BINDIR)/LIST $(BINDIR)/FILE $(BINDIR)/KEYS $(BINDIR)/WRITE $(BINDIR)/UPDATE $(BINDIR)/DEL $(BINDIR)/DELa check-linker-path
	install -d $(INCLUDEDIR)
	install -m 644 include/types.h include/hash_tbl.h include/file.h include/str_op.h include/record.h include/parse.h include/lock.h include/crud.h $(INCLUDEDIR)/
	install -m 755 $(SHAREDLIBht) $(LIBDIR)
	install -m 755 $(SHAREDLIBf) $(LIBDIR)
	install -m 755 $(SHAREDLIBs) $(LIBDIR)
	install -m 755 $(SHAREDLIBr) $(LIBDIR)
	install -m 755 $(SHAREDLIBp) $(LIBDIR) 
	install -m 755 $(SHAREDLIBl) $(LIBDIR)
	install -m 755 $(SHAREDLIBexpl) $(LIBDIR)
	ldconfig
build: object-dir default library install

build_prod: object-dir prod libraryPR install_prod 

.PHONY: default test memory clean install library check-linker-path object-dir prod $(BINDIR)/SHOW $(BINDIR)/LIST $(BINDIR)/FILE $(BINDIR)/KEYS $(BINDIR)/WRITE $(BINDIR)/UPDATE $(BINDIR)/DEL $(BINDIR)/DELa
