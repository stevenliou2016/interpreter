CC = gcc
CFLAGS = -g -c
OBJS = interpreter.o command_line.o mem_manage.o console.o queue.o client.o rio.o

interpreter: $(OBJS)
	$(CC) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@


.PHONY: clean

clean:
	rm interpreter $(OBJS)
