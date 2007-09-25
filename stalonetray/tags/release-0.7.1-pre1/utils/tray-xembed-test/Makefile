CFLAGS = -g -O2

SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)

TARGET = tray-xembed-test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ -lX11

clean:
	rm -rf *.o $(TARGET)
