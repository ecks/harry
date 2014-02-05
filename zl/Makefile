CC = gcc
CFLAGS = -ggdb -I../lib -I.
EXECUTABLE = zebralite
SRCS = api.c netlink.c listener.c vconn.c vconn-stream.c stream.c stream-fd.c stream-tcp.c util.c socket-util.c rfp-msgs.c rfpbuf.c dblist.c datapath.c rconn.c poll-loop.c timeval.c fatal-signal.c thread.c zl_serv.c controller.c log.c command.c vty.c vector.c debug.c
OBJECTS = ${SRCS:.c=.o}
LIBS = -lrt
VPATH = ../lib

all: $(EXECUTABLE)

zebralite: zebralite.o $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ zebralite.o $(OBJECTS) $(LIBS)

api.o: api.c
	$(CC) $(CFLAGS) -c $^

netlink.o: netlink.c
	$(CC) $(CFLAGS) -c $^

listener.o: listener.c
	$(CC) $(CFLAGS) -c $^

vconn.o: vconn.c
	$(CC) $(CFLAGS) -c $^

vconn-stream.o: vconn-stream.c
	$(CC) $(CFLAGS) -c $^

stream.o: stream.c
	$(CC) $(CFLAGS) -c $^

stream-fd.o: stream-fd.c
	$(CC) $(CFLAGS) -c $^

stream-tcp.o: stream-tcp.c
	$(CC) $(CFLAGS) -c $^

util.o: util.c
	$(CC) $(CFLAGS) -c $^

socket-util.o: socket-util.c
	$(CC) $(CFLAGS) -c $^

rfp-msgs.o: rfp-msgs.c
	$(CC) $(CFLAGS) -c $^

rfpbuf.o: rfpbuf.c
	$(CC) $(CFLAGS) -c $^

dblist.o: dblist.c
	$(CC) $(CFLAGS) -c $^

datapath.o: datapath.c
	$(CC) $(CFLAGS) -c $^

rconn.o: rconn.c
	$(CC) $(CFLAGS) -c $^

poll-loop.o: poll-loop.c
	$(CC) $(CFLAGS) -c $^

timeval.o: timeval.c
	$(CC) $(CFLAGS) -c $^

fatal-signal.o: fatal-signal.c
	$(CC) $(CFLAGS) -c $^

thread.o: thread.c
	$(CC) $(CFLAGS) -c $^

zl_serv.o: zl_serv.c
	$(CC) $(CFLAGS) -c $^

controller.o: controller.c
	$(CC) $(CFLAGS) -c $^

log.o: log.c
	$(CC) $(CFLAGS) -c $^

command.o: command.c
	$(CC) $(CFLAGS) -c $^

vty.o: vty.c
	$(CC) $(CFLAGS) -c $^

vector.o: vector.c
	$(CC) $(CFLAGS) -c $^

debug.o: debug.c
	$(CC) $(CFLAGS) -c $^

.c.o:
	$(CC) $(CFLAGS) -c $*.c

clean:
	/bin/rm -f *.o $(EXECUTABLE)
