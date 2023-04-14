CC = gcc
CFLAGS = -g -pthread

# List all the source files
SRCS = processor.c processor.h libpng16.a libz.a

# Generate a list of object files from the source files
OBJS = $(SRCS:.c=.o)

# The executable file to be generated
TARGET = processor

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lrt -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f processor processor.o