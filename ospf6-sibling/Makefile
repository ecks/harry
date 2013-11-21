CC = gcc
CXX = g++
CFLAGS = -ggdb -I. -I../lib -I../../riack/src -I/usr/local/include
EXECUTABLE = ospf6-sibling
LIBS = -pthread -lrt ../../riack/libriack.a -ljansson -L/usr/local/lib -L/home/hasenov/cpputest_lib/lib -lstdc++ -lCppUTest -lCppUTestExt
SRCS = vconn.c vconn-stream.c stream.c stream-tcp.c stream-fd.c rfpbuf.c rfp-msgs.c dblist.c socket-util.c util.c poll-loop.c timeval.c fatal-signal.c prefix.c sisis.c sisis_api.c netlink.c sibling_ctrl.c ctrl_client.c thread.c if.c ospf6_interface.c ospf6_top.c ospf6_message.c ospf6_neighbor.c ospf6_replica.c ospf6_lsdb.c ospf6_lsa.c ospf6_flood.c ospf6_route.c ospf6_area.c log.c ospf6_intra.c ospf6_db.c command.c vty.c debug.c vector.c table.c
OBJECTS = ${SRCS:.c=.o}
VPATH = ../lib

CFLAGS_CPPUTEST = -I../lib -I. -I/home/hasenov/cpputest_lib/include
FAKE_SRCS = fake_ospf6_top.c fake_ospf6_replica.c
FAKE_OBJECTS_ONLY = ${FAKE_SRCS:.c=.o}
FAKE_OBJECTS_ALL = $(subst ospf6_replica, fake_ospf6_replica, $(subst ospf6_top, fake_ospf6_top, $(OBJECTS)))
OBJECTS_CPPUTEST = $(OBJECTS:ospf6_top=fake_ospf6_top)

all: ${EXECUTABLE}

test: all ${FAKE_OBJECTS_ONLY}
	$(CXX) $(CFLAGS_CPPUTEST) -o $@ tests/database_test_run.cpp tests/database_test.cpp $(FAKE_OBJECTS_ALL) $(LIBS)
#	./$@

fake_ospf6_top.o: tests/fake_ospf6_top.c
	$(CC) $(CFLAGS) -c $^	

fake_ospf6_replica.o: tests/fake_ospf6_replica.c
	$(CC) $(CFLAGS) -c $^

ospf6-sibling: ospf6-sibling.o ${OBJECTS}
	$(CC) $(CFLAGS) -o $@ ospf6-sibling.o $(OBJECTS) $(LIBS)

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

sisis.o: sisis.c
	$(CC) $(CFLAGS) -c $^

sisis_api.o: sisis_api.c
	$(CC) $(CFLAGS) -c $^

netlink.o: netlink.c
	$(CC) $(CFLAGS) -c $^

sibling_ctrl.o: sibling_ctrl.c
	$(CC) $(CFLAGS) -c $^

ctrl_client.o: ctrl_client.c
	$(CC) $(CFLAGS) -c $^

thread.o: thread.c
	$(CC) $(CFLAGS) -c $^

if.o: if.c
	$(CC) $(CFLAGS) -c $^

ospf6_interface.o: ospf6_interface.c
	$(CC) $(CFLAGS) -c $^

ospf6_top.o: ospf6_top.c
	$(CC) $(CFLAGS) -c $^

ospf6_message.o: ospf6_message.c
	$(CC) $(CFLAGS) -c $^

ospf6_neighbor.o: ospf6_neighbor.c
	$(CC) $(CFLAGS) -c $^

ospf6_replica.o: ospf6_replica.c
	$(CC) $(CFLAGS) -c $^

ospf6_lsdb.o: ospf6_lsdb.c
	$(CC) $(CFLAGS) -c $^

ospf6_lsa.o: ospf6_lsa.c
	$(CC) $(CFLAGS) -c $^

ospf6_flood.o: ospf6_flood.c
	$(CC) $(CFLAGS) -c $^

ospf6_route.o: ospf6_route.c
	$(CC) $(CFLAGS) -c $^

ospf6_intra.o: ospf6_intra.c
	$(CC) $(CFLAGS) -c $^

ospf6_area.o: ospf6_area.c
	$(CC) $(CFLAGS) -c $^

ospf6_db.o: ospf6_db.c
	$(CC) $(CFLAGS) -c $^


log.o: log.c
	$(CC) $(CFLAGS) -c $^

command.o: command.c
	$(CC) $(CFLAGS) -c $^

vty.o: vty.c
	$(CC) $(CFLAGS) -c $^

debug.o: debug.c
	$(CC) $(CFLAGS) -c $^

vector.o: vector.c
	$(CC) $(CFLAGS) -c $^

table.o: table.c
	$(CC) $(CFLAGS) -c $^

clean:
	/bin/rm -f *.o ${EXECUTABLE}
