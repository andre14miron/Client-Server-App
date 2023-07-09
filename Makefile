CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

all: server subscriber

# Compiling server.c
server: -lm server.c

# Compiling subscriber.c
subscriber: -lm subscriber.c

.PHONY: clean common run_server run_subscriber

clean:
	rm -f server subscriber *.o