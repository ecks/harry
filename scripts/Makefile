CC = cc
CFLAGS = -ggdb -I. -I../lib -I../../riack/src -I/usr/local/include
EXECUTABLE = clean_db reset_key
SRCS =
OBJECTS = ${SRCS:.c=.o}
LIBS = ../../riack/libriack.a

all: ${EXECUTABLE}

clean_db: 
	$(CC) $(CFLAGS) -o $@ clean_db.c $(LIBS)

reset_key:
	$(CC) $(CFLAGS) -o $@ reset_key.c $(LIBS)
