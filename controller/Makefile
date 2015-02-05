CC = gcc
CFLAGS = -ggdb -I../lib -I../../etcd-api
LIBS = -pthread -lrt -letcd -L../../etcd-api
EXECUTABLE = controller
SRCS = vconn.c rconn.c vconn-stream.c util.c rfpbuf.c rfp-msgs.c dblist.c stream-fd.c stream.c socket-util.c stream-tcp.c poll-loop.c timeval.c fatal-signal.c router.c sib_router.c table.c prefix.c rib.c vector.c sisis.c sisis_api.c netlink.c if.c thread.c log.c command.c vty.c debug.c etcd.c
OBJECTS = ${SRCS:.c=.o}
VPATH = ../lib

all: $(EXECUTABLE)

controller: controller.o $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ controller.o $(OBJECTS) $(LIBS)

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

sisis.o: sisis.c
	$(CC) $(CFLAGS) $(LIBS) -c $^

sisis_api.o: sisis_api.c
	$(CC) $(CFLAGS) $(LIBS) -c $^

netlink.o: netlink.c
	$(CC) $(CFLAGS) $(LIBS) -c $^

if.o: if.c
	$(CC) $(CFLAGS) $(LIBS) -c $^

thread.o: thread.c
	$(CC) $(CFLAGS) $(LIBS) -c $^

log.o: log.c
	$(CC) $(CFLAGS) $(LIBS) -c $^

command.o: command.c
	$(CC) $(CFLAGS) $(LIBS) -c $^

vty.o: vty.c
	$(CC) $(CFLAGS) $(LIBS) -c $^

debug.o: debug.c
	$(CC) $(CFLAGS) $(LIBS) -c $^

etcd.o: etcd.c
	$(CC) $(CFLAGS) $(LIBS) -c $^

.c.o:
	$(CC) $(CFLAGS) -c $*.c

clean:
	/bin/rm -f *.o $(EXECUTABLE)
