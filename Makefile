CC=clang
CFLAGS=-Wall -Wextra -Werror -pedantic

all: httpserver

httpserver: httpserver.o process_request.o post_request.o asgn4_helper_funcs.a
	${CC} -o httpserver httpserver.o process_request.o post_request.o asgn4_helper_funcs.a ${CFLAGS}

httpserver.o: httpserver.c
	${CC} -c httpserver.c ${CFLAGS}

process_request.o: process_request.c
	${CC} -c process_request.c ${CFLAGS}

post_request.o: post_request.c
	${CC} -c post_request.c ${CFLAGS}

clean:
	rm -f httpserver *.o
