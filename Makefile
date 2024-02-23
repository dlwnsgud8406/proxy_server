OBJS = proxy_cache.c
CC = gcc
EXEC=proxy_cache

all: $(OBJS)
	$(CC) -pthread -o $(EXEC) $(OBJS) -lcrypto
clean:
	rm -rf $(EXEC)
