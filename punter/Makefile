CC = gcc
CFLAGS = -ggdb -I../lib -D_GNU_SOURCE
EXECUTABLE = punter
LIBS = -lrt
SRCS = punter.c punter_ctrl.c ext_client.c ext_client_ospf6.c ext_client_bgp.c zl_client.c zl_client_network.c socket-util.c util.c thread.c rfp-msgs.c rfpbuf.c dblist.c api.c netlink.c sockunion.c
OBJECTS = ${SRCS:.c=.o}
VPATH = ../lib

all: $(EXECUTABLE)

punter: punter.o $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LIBS)

punter_ctrl.o: punter_ctrl.c
	$(CC) $(CFLAGS) -c $^

ext_client.o: ext_client.c
	$(CC) $(CFLAGS) -c $^

ext_client_ospf6.o: ext_client_ospf6.c
	$(CC) $(CFLAGS) -c $^

zl_client.o: zl_client.c
	$(CC) $(CFLAGS) -c $^

thread.o: thread.c
	$(CC) $(CFLAGS) -c $^

rfp-msgs.o: rfp-msgs.c
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

sockunion.o: sockunion.c
	$(CC) $(CFLAGS) -c $^

.c.o:
	$(CC) $(CFLAGS) -c $*.c

clean:
	/bin/rm -f *.o $(EXECUTABLE)
