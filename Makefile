CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = p


# The default target to build
all: $(TARGET)


# Target to compile projectBFS_2.c into the p executable
$(TARGET): projectBFS_2.c
	$(CC) $(CFLAGS) -o $(TARGET) projectBFS_2.c


# Clean up any generated files
clean:
	rm -f $(TARGET)
