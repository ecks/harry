CC = gcc
CFLAGS = -ggdb
EXECUTABLE = controller
API_OBJECTS = vconn.o rconn.o vconn-stream.o util.o rfpbuf.o rfp-msgs.o dblist.o stream-fd.o stream.o socket-util.o stream-tcp.o poll-loop.o timeval.o fatal-signal.o router.o

all: $(EXECUTABLE)

controller: controller.o $(API_OBJECTS)
	$(CC) $(CFLAGS) -o $@ controller.o $(API_OBJECTS)

vconn.o:
	$(CC) $(CFLAGS) -c ../lib/vconn.c

rconn.o:
	$(CC) $(CFLAGS) -c ../lib/rconn.c

vconn-stream.o:
	$(CC) $(CFLAGS) -c ../lib/vconn-stream.c

stream-fd.o:
	$(CC) $(CFLAGS) -c ../lib/stream-fd.c

stream.o:
	$(CC) $(CFLAGS) -c ../lib/stream.c

util.o:
	$(CC) $(CFLAGS) -c ../lib/util.c

rfpbuf.o:
	$(CC) $(CFLAGS) -c ../lib/rfpbuf.c

rfp-msgs.o:
	$(CC) $(CFLAGS) -c ../lib/rfp-msgs.c

dblist.o:
	$(CC) $(CFLAGS) -c ../lib/dblist.c

socket-util.o:
	$(CC) $(CFLAGS) -c ../lib/socket-util.c

stream-tcp.o:
	$(CC) $(CFLAGS) -c ../lib/stream-tcp.c

poll-loop.o:
	$(CC) $(CFLAGS) -c ../lib/poll-loop.c

timeval.o:
	$(CC) $(CFLAGS) -c ../lib/timeval.c

router.o:
	$(CC) $(CFLAGS) -c ../lib/router.c

fatal-signal.o:
	$(CC) $(CFLAGS) -c ../lib/fatal-signal.c
.c.o:
	$(CC) $(CFLAGS) -c $*.c

clean:
	/bin/rm -f *.o $(EXECUTABLE)
