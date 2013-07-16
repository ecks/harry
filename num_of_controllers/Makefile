CC = gcc
CFLAGS = -I../lib
EXECUTABLE = num_of_controllers
SRCS = num_of_controllers.c erl_comm.c prefix.c sisis.c sisis_api.c netlink.c dblist.c
OBJECTS = ${SRCS:.c=.o}
LIBS = -lpthread -lrt 
VPATH = ../lib

all: $(EXECUTABLE)

num_of_controllers: num_of_controllers.o $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LIBS)

erl_comm.o: erl_comm.c
	$(CC) $(CFLAGS) -c $^

prefix.o: prefix.c
	$(CC) $(CFLAGS) -c $^

sisis.o: sisis.c
	$(CC) $(CFLAGS) -c $^

sisis_api.o: sisis_api.c
	$(CC) $(CFLAGS) -c $^

netlink.o: netlink.c
	$(CC) $(CFLAGS) -c $^

dblist.o: dblist.c
	$(CC) $(CFLAGS) -c $^

.c.o:
	$(CC) $(CFLAGS) -c $*.c
