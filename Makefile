LIBS = -ll -ly -lreadline -lcurses
SRCS = main.c main.h
CFLAGS = -Wall -Wextra
CC = gcc

all: msh

msh: msh.l msh.y $(SRCS)
	bison -d msh.y
	flex msh.l
	$(CC) -o $@ msh.tab.c lex.yy.c $(SRCS) $(LIBS)

clean:
	rm -f lex.yy.c msh.tab.c msh.tab.h msh
