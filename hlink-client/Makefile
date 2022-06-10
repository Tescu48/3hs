
OBJS = hlink.o main.o
CFLAGS = -pedantic -Wall
PREFIX = /usr/local/bin
TARGET = hlink

.PHONY: clean all install
all: $(TARGET)
clean:
	@rm -f $(OBJS) hlink

install: $(TARGET)
	install -m 557 $(TARGET) $(PREFIX)

%.o: %.c
	$(CC) -c $(<) -o $(@) $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) $(^) -o hlink $(CFLAGS)

