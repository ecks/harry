CC = gcc
CFLAGS = -ggdb
EXECUTABLE = zebralite
API_OBJECTS = api.o netlink.o listener.o Class.o list.o RouteV4.o vconn.o vconn-stream.o stream.o stream-fd.o stream-tcp.o util.o socket-util.o rfp-msgs.o rfpbuf.o dblist.o datapath.o rconn.o poll-loop.o timeval.o fatal-signal.o
LIBS = -lrt

all: $(EXECUTABLE)

zebralite: zebralite.o $(API_OBJECTS)
	$(CC) $(CFLAGS) $(LIBS) -o $@ zebralite.o $(API_OBJECTS)

list.o:
	$(CC) $(CFLAGS) -c lib/list.c

Class.o:
	$(CC) $(CFLAGS) -c lib/Class.c

RouteV4.o:
	$(CC) $(CFLAGS) -c lib/RouteV4.c

vconn.o:
	$(CC) $(CFLAGS) -c lib/vconn.c

vconn-stream.o:
	$(CC) $(CFLAGS) -c lib/vconn-stream.c

stream.o:
	$(CC) $(CFLAGS) -c lib/stream.c

stream-fd.o:
	$(CC) $(CFLAGS) -c lib/stream-fd.c

stream-tcp.o:
	$(CC) $(CFLAGS) -c lib/stream-tcp.c

util.o:
	$(CC) $(CFLAGS) -c lib/util.c

socket-util.o:
	$(CC) $(CFLAGS) -c lib/socket-util.c

rfp-msgs.o:
	$(CC) $(CFLAGS) -c lib/rfp-msgs.c

rfpbuf.o:
	$(CC) $(CFLAGS) -c lib/rfpbuf.c

dblist.o:
	$(CC) $(CFLAGS) -c lib/dblist.c

datapath.o:
	$(CC) $(CFLAGS) -c datapath.c

rconn.o:
	$(CC) $(CFLAGS) -c lib/rconn.c

poll-loop.o:
	$(CC) $(CFLAGS) -c lib/poll-loop.c

timeval.o:
	$(CC) $(CFLAGS) -c lib/timeval.c

fatal-signal.o:
	$(CC) $(CFLAGS) -c lib/fatal-signal.c

.c.o:
	$(CC) $(CFLAGS) -c $*.c

clean:
	/bin/rm -f *.o $(EXECUTABLE)
