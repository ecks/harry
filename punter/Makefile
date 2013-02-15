CC = gcc
CFLAGS = -ggdb -I../lib -D_GNU_SOURCE
EXECUTABLE = punter
SRCS = punter.c punter_ctrl.c ext_client.c ext_client_network.c zl_client.c  socket-util.c util.c thread.c rfpbuf.c dblist.c api.c netlink.c
OBJECTS = ${SRCS:.c=.o}
LIBS = -lrt
VPATH = ../lib

all: $(EXECUTABLE)

punter: punter.o $(OBJECTS)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $(OBJECTS)

punter_ctrl.o: punter_ctrl.c
	$(CC) $(CFLAGS) -c $^

ext_client.o: ext_client.c
	$(CC) $(CFLAGS) -c $^

ext_client_network.o: ext_client_network.c
	$(CC) $(CFLAGS) -c $^

zl_client.o: zl_client.c
	$(CC) $(CFLAGS) -c $^

thread.o: thread.c
	$(CC) $(CFLAGS) -c $^

rfpbuf.o: rfpbuf.c
	$(CC) $(CFLAGS) -c $^

dblist.o: dblist.c
	$(CC) $(CFLAGS) -c $^

api.o: api.c
	$(CC) $(CFLAGS) -c $^

netlink.o: netlink.c
	$(CC) $(CFLAGS) -c $^

util.o: util.c
	$(CC) $(CFLAGS) -c $^

socket-util.o: socket-util.c
	$(CC) $(CFLAGS) -c $^

.c.o:
	$(CC) $(CFLAGS) -c $*.c

clean:
	/bin/rm -f *.o $(EXECUTABLE)
