Project = interpreter
Program = interpreter
CC = gcc
CFLAGS = -g -c
#OBJS = interpreter.o command_line.o mem_manage.o console.o queue.o client/client.o rio.o server.o messages.o
OBJS = $(Project).o $(Project)_cmd_line.o $(Project)_mem.o $(Project)_console.o $(Project)_queue.o client/$(Project)_client.o $(Project)_rio.o $(Project)_server.o $(Project)_msg.o

$(Program): $(OBJS)
	$(CC) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@


.PHONY: clean test

test: $(Program) scripts/test.py
	scripts/test.py -c

clean:
	rm $(Program) $(OBJS)
