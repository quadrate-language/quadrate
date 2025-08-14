CC      := gcc
CFLAGS  := -I../include -Wall -Wextra
LDFLAGS := -L../lib -lquadrate

TARGET  := qdapi_test

.PHONY: all clean run

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c $(LDFLAGS)

run: $(TARGET)
	LD_LIBRARY_PATH=../lib ./$(TARGET)

clean:
	$(RM) $(TARGET)
