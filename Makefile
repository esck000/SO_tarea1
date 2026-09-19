CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11

TARGET = mishell

OBJS = mishell.o parser.o builtins.o jobs.o executor.o pmon.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

mishell.o: mishell.c parser.h builtins.h jobs.h executor.h
parser.o: parser.c parser.h
builtins.o: builtins.c builtins.h jobs.h
jobs.o: jobs.c jobs.h
executor.o: executor.c executor.h jobs.h
pmon.o: pmon.c pmon.h jobs.h

clean:
	rm -f $(OBJS) $(TARGET)