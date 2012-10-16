CC = gcc
CFLAGS = -ggdb -I../lib
EXECUTABLE = controller
SRCS = vconn.c rconn.c vconn-stream.c util.c rfpbuf.c rfp-msgs.c dblist.c stream-fd.c stream.c socket-util.c stream-tcp.c poll-loop.c timeval.c fatal-signal.c router.c sib_router.c table.c prefix.c rib.c vector.c
OBJECTS = ${SRCS:.c=.o}
VPATH = ../lib

all: $(EXECUTABLE)

controller: controller.o $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ controller.o $(OBJECTS)

vconn.o: vconn.c
	$(CC) $(CFLAGS) -c $^

rconn.o: rconn.c
	$(CC) $(CFLAGS) -c $^

vconn-stream.o: vconn-stream.c
	$(CC) $(CFLAGS) -c $^

stream-fd.o: stream-fd.c
	$(CC) $(CFLAGS) -c $^

stream.o: stream.c
	$(CC) $(CFLAGS) -c $^

util.o: util.c
	$(CC) $(CFLAGS) -c $^

rfpbuf.o: rfpbuf.c
	$(CC) $(CFLAGS) -c $^

rfp-msgs.o: rfp-msgs.c
	$(CC) $(CFLAGS) -c $^

dblist.o: dblist.c
	$(CC) $(CFLAGS) -c $^

socket-util.o: socket-util.c
	$(CC) $(CFLAGS) -c $^

stream-tcp.o: stream-tcp.c
	$(CC) $(CFLAGS) -c $^

poll-loop.o: poll-loop.c
	$(CC) $(CFLAGS) -c $^

timeval.o: timeval.c
	$(CC) $(CFLAGS) -c $^

router.o: router.c
	$(CC) $(CFLAGS) -c $^

sib_router.o: sib_router.c
	$(CC) $(CFLAGS) -c $^

fatal-signal.o: fatal-signal.c
	$(CC) $(CFLAGS) -c $^

table.o: table.c
	$(CC) $(CFLAGS) -c $^

prefix.o: prefix.c
	$(CC) $(CFLAGS) -c $^

rib.o: rib.c
	$(CC) $(CFLAGS) -c $^

vector.o: vector.c
	$(CC) $(CFLAGS) -c $^

.c.o:
	$(CC) $(CFLAGS) -c $*.c

clean:
	/bin/rm -f *.o $(EXECUTABLE)
