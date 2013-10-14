CC = gcc
CFLAGS = -ggdb -I../lib
EXECUTABLE = bgp-sibling
LIBS = -pthread -lrt
SRCS = rfpbuf.c rfp-msgs.c prefix.c sisis.c sisis_api.c thread.c dblist.c netlink.c bgp_sibling_ctrl.c bgp_ctrl_client.c socket-util.c util.c if.c log.c command.c vty.c vector.c
OBJECTS = $(SRCS:.c=.o)
VPATH = ../lib

all: ${EXECUTABLE}

bgp-sibling: bgp-sibling.o ${OBJECTS}
	$(CC) $(CFLAGS) -o $@ bgp-sibling.o $(OBJECTS) $(LIBS)

prefix.o: prefix.c
	$(CC) $(CFLAGS) -c $^

sisis.o: sisis.c
	$(CC) $(CFLAGS) -c $^

sisis_api.o: sisis_api.c
	$(CC) $(CFLAGS) -c $^

thread.o: thread.c
	$(CC) $(CFLAGS) -c $^

dblist.o: dblist.c
	$(CC) $(CFLAGS) -c $^

netlink.o: netlink.c
	$(CC) $(CFLAGS) -c $^

bgp_sibling_ctrl.o: bgp_sibling_ctrl.c
	$(CC) $(CFLAGS) -c $^

bgp_ctrl_client.o: bgp_ctrl_client.c
	$(CC) $(CFLAGS) -c $^

socket-util.o: socket-util.c
	$(CC) $(CFLAGS) -c $^

log.o: log.c
	$(CC) $(CFLAGS) -c $^

command.o: command.c
	$(CC) $(CFLAGS) -c $^

vty.o: vty.c
	$(CC) $(CFLAGS) -c $^

vector.o: vector.c
	$(CC) $(CFLAGS) -c $^

util.o: util.c
	$(CC) $(CFLAGS) -c $^

rfpbuf.o: rfpbuf.c
	$(CC) $(CFLAGS) -c $^

rfpf-msgs.o: rfp-msgs.c
	$(CC) $(CFLAGS) -c $^

if.o: if.c
	$(CC) $(CFLAGS) -c $^

clean:
	/bin/rm -f *.o $(EXECUTABLE)
