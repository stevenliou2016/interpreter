CC = gcc
CFLAGS = -g -c
OBJS = interpreter.o command_line.o mem_manage.o console.o queue.o client/client.o rio.o server.o

interpreter: $(OBJS)
	$(CC) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@


.PHONY: clean test

test: interpreter scripts/test.py
	scripts/test.py -c

clean:
	rm interpreter $(OBJS)
