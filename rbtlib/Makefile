CORE = rabbit.o gc.o mem.o math.o object.o db.o db_mgr.o config.o remote_call.o time_process_list.o least_recently_used.o fill_stream.o util.o md5.o array.o open_login.o base64.o
DS = queue.o table.o string.o buffer.o pool.o statistic.o array_map.o
AMF = amf_common.o decoder.o encoder.o
PHP_COdE = php_serialize.o php_deserialize.o
IO = connection.o io.o mblock.o rawbuffer.o
FILE = stream.o
NET = net_manager.o packet.o zredis.o
SCRIPT = script.o parser.o lexical.o code.o proto.o opcode.o vm.o script_base_lib.o script_db_lib.o script_load.o script_misc.o
#COMPRESS = ../Compress/hash.o ../Compress/bit.o ../Compress/lzw.o

CC = gcc 
CFLAG= -c -O2 -Wall -I/opt/ci123/mysql/include/mysql -fPIC
LCFLAG= -lcurses -L/opt/ci123/mysql/lib/mysql -L/usr/lib64/mysql -lmysqlclient -lefence

all:$(CORE) $(DS) $(AMF) $(IO) $(MD5) $(FILE) $(NET) $(SCRIPT) $(PHP_COdE) $(COMPRESS)

#core
rabbit.o: rabbit.c rabbit.h common.h
	$(CC) $(CFLAG) -o $@ $<

gc.o: gc.c gc.h object.h
	$(CC) $(CFLAG) -o $@ $<

mem.o: mem.c mem.h
	$(CC) $(CFLAG) -o $@ $<

math.o: math.c math.h
	$(CC) $(CFLAG) -o $@ $<

object.o: object.c object.h
	$(CC) $(CFLAG) -o $@ $<

util.o: util.c util.h
	$(CC) $(CFLAG) -o $@ $<

db.o: db.c db.h
	$(CC) $(CFLAG) -o $@ $<

db_mgr.o: db_mgr.c db_mgr.h
	$(CC) $(CFLAG) -o $@ $<

config.o: config.c config.h
	$(CC) $(CFLAG) -o $@ $<

remote_call.o: remote_call.c remote_call.h
	$(CC) $(CFLAG) -o $@ $<

time_process_list.o: time_process_list.c time_process_list.h
	$(CC) $(CFLAG) -o $@ $<

least_recently_used.o: least_recently_used.c least_recently_used.h
	$(CC) $(CFLAG) -o $@ $<

fill_stream.o: fill_stream.c fill_stream.h
	$(CC) $(CFLAG) -o $@ $<

md5.o: md5.c md5.h
	$(CC) $(CFLAG) -o $@ $<

open_login.o: open_login.c open_login.h
	$(CC) $(CFLAG) -o $@ $<

array.o: array.c array.h
	$(CC) $(CFLAG) -o $@ $<

#data structure
queue.o: queue.c queue.h
	$(CC) $(CFLAG) -o $@ $<

buffer.o: buffer.c buffer.h
	$(CC) $(CFLAG) -o $@ $<

string.o: string.c string.h object.h
	$(CC) $(CFLAG) -o $@ $<

table.o: table.c table.h object.h
	$(CC) $(CFLAG) -o $@ $<

pool.o: pool.c pool.h
	$(CC) $(CFLAG) -o $@ $<

statistic.o: statistic.c statistic.h
	$(CC) $(CFLAG) -o $@ $<

array_map.o: array_map.c array_map.h list.h
	$(CC) $(CFLAG) -o $@ $<

#AMF
amf_common.o: amf_common.c amf_common.h
	$(CC) $(CFLAG) -o $@ $<

decoder.o: decoder.c amf_common.h
	$(CC) $(CFLAG) -o $@ $<

encoder.o: encoder.c amf_common.h
	$(CC) $(CFLAG) -o $@ $<

#PHP Serialize
php_serialize.o: php_serialize.c
	$(CC) $(CFLAG) -o $@ $<

php_deserialize.o: php_deserialize.c
	$(CC) $(CFLAG) -o $@ $<

#io
io.o: io.c io.h
	$(CC) $(CFLAG) -o $@ $<

mblock.o: mblock.c mblock.h io.h
	$(CC) $(CFLAG) -o $@ $<

rawbuffer.o: rawbuffer.c rawbuffer.h
	$(CC) $(CFLAG) -o $@ $<

#net
net_manager.o: net_manager.c net_manager.h object.h
	$(CC) $(CFLAG) -o $@ $<

packet.o: packet.c packet.h object.h
	$(CC) $(CFLAG) -o $@ $<

connection.o: connection.c connection.h
	$(CC) $(CFLAG) -o $@ $<

zredis.o: zredis.c zredis.h
	$(CC) $(CFLAG) -o $@ $<

#file
stream.o: stream.c stream.h
	$(CC) $(CFLAG) -o $@ $<

#script
script.o: script.c script.h
	$(CC) $(CFLAG) -o $@ $<

parser.o: parser.c parser.h
	$(CC) $(CFLAG) -o $@ $<

lexical.o: lexical.c lexical.h
	$(CC) $(CFLAG) -o $@ $<

code.o: code.c code.h
	$(CC) $(CFLAG) -o $@ $<

proto.o: proto.c proto.h
	$(CC) $(CFLAG) -o $@ $<

opcode.o: opcode.c opcode.h
	$(CC) $(CFLAG) -o $@ $<

vm.o: vm.c vm.h
	$(CC) $(CFLAG) -o $@ $<

script_base_lib.o: script_base_lib.c script_base_lib.h
	$(CC) $(CFLAG) -o $@ $<

script_db_lib.o: script_db_lib.c script_db_lib.h
	$(CC) $(CFLAG) -o $@ $<

script_load.o: script_load.c script_load.h
	$(CC) $(CFLAG) -o $@ $<

script_misc.o: script_misc.c script_misc.h
	$(CC) $(CFLAG) -o $@ $<

# compress
../Compress/hash.o: ../Compress/hash.c ../Compress/interface.h
	$(CC) $(CFLAG) -o $@ $<

../Compress/bit.o: ../Compress/bit.c ../Compress/interface.h
	$(CC) $(CFLAG) -o $@ $<

../Compress/lzw.o: ../Compress/lzw.c ../Compress/interface.h
	$(CC) $(CFLAG) -o $@ $<

clean:
	@-rm -rf *.o
	@-rm -rf Compress/*.o
