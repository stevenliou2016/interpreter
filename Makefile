CC = gcc
CFLAGS = -g -c
OBJS = client.o rio.o

client: $(OBJS)
	$(CC) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@


.PHONY: clean

clean:
	rm client $(OBJS)
