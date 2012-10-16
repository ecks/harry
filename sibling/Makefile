CC = gcc
CFLAGS = -ggdb -I../lib
EXECUTABLE = sibling
SRCS = vconn.c vconn-stream.c stream.c stream-tcp.c stream-fd.c rfpbuf.c rfp-msgs.c dblist.c socket-util.c util.c poll-loop.c timeval.c fatal-signal.c prefix.c
OBJECTS = ${SRCS:.c=.o}
VPATH = ../lib

all: ${EXECUTABLE}

sibling: sibling.o ${OBJECTS}
	$(CC) $(CFLAGS) -o $@ sibling.o $(OBJECTS)

vconn.o: vconn.c
	$(CC) $(CFLAGS) -c $^

vconn-stream.o: vconn-stream.c
	$(CC) $(CFLAGS) -c $^

stream.o: stream.c
	$(CC) $(CFLAGS) -c $^

stream-tcp.o: stream-tcp.c
	$(CC) $(CFLAGS) -c $^

stream-fd.o: stream-fd.c
	$(CC) $(CFLAGS) -c $^

rfpbuf.o: rfpbuf.c
	$(CC) $(CFLAGS) -c $^

dblist.o: dblist.c
	$(CC) $(CFLAGS) -c $^

rfp-msgs.o: rfp-msgs.c
	$(CC) $(CFLAGS) -c $^

socket-util.o: socket-util.c
	$(CC) $(CFLAGS) -c $^

util.o: util.c
	$(CC) $(CFLAGS) -c $^

poll-loop.o: poll-loop.c
	$(CC) $(CFLAGS) -c $^

timeval.o: timeval.c
	$(CC) $(CFLAGS) -c $^

fatal-signal.o: fatal-signal.c
	$(CC) $(CFLAGS) -c $^

prefix.o: prefix.c
	$(CC) $(CFLAGS) -c $^

clean:
	/bin/rm -f *.o ${EXECUTABLE}
