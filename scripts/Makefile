CC = cc
CFLAGS = -ggdb -I. -I../lib -I../../riack/src -I/usr/local/include
EXECUTABLE = clean_db
SRCS = db.c
OBJECTS = ${SRCS:.c=.o}
LIBS = ../../riack/libriack.a

all: ${EXECUTABLE}

clean_db: 
	$(CC) $(CFLAGS) -o $@ clean_db.c ../lib/db.c $(LIBS)
