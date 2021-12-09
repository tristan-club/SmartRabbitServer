CC = gcc
FLAG = -c -Wall -g -O0

LFLAG = -lcurses #-lcrypto

LIB = ../rbtlib/*.o
CORE = main.o server.o player.o svr_connection.o boot.o  process.o twice_login.o

RPC = rpc.o

RPC_SERVER = handle_server.o handle_admin.o

RPC_CLIENT = handle_client.o 

all: $(CORE) $(RPC) $(RPC_SERVER) $(RPC_CLIENT)
	@make -C ../rbtlib/ CFLAG='$(FLAG)'
	$(CC) $(LFLAG) $(CORE) $(LIB) $(RPC) $(RPC_SERVER) $(RPC_CLIENT)-o rbt_rabbit

#core
main.o: main.c server.h
	$(CC) $(FLAG) -o $@ $<

server.o: server.c server.h
	$(CC) $(FLAG) -o $@ $<

player.o: player.c player.h
	$(CC) $(FLAG) -o $@ $<

svr_connection.o: svr_connection.c svr_connection.h
	$(CC) $(FLAG) -o $@ $<

boot.o: boot.c boot.h
	$(CC) $(FLAG) -o $@ $<

login.o: login.c login.h
	$(CC) $(FLAG) -o $@ $<

base64.o: base64.c base64.h
	$(CC) $(FLAG) -o $@ $<

process.o: process.c process.h
	$(CC) $(FLAG) -o $@ $<

twice_login.o: twice_login.c twice_login.h
	$(CC) $(FLAG) -o $@ $<

#rpc
rpc.o: rpc.c rpc.h
	$(CC) $(FLAG) -o $@ $<

handle_client.o: handle_client.c rpc.h
	$(CC) $(FLAG) -o $@ $<

handle_server.o: handle_server.c rpc.h
	$(CC) $(FLAG) -o $@ $<

handle_admin.o: handle_admin.c rpc.h
	$(CC) $(FLAG) -o $@ $<

#clean
clean:
	@make -s -C ../rbtlib/ clean
	@-rm -rf *.o
