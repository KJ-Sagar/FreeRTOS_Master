CC = gcc
CFLAGS = -Wall -Wextra -O2

all: someip_client

someip_client: someip_client.c someip_protocol.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f someip_client
