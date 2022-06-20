
OBJS = hlink.o main.o hstx.o
CFLAGS = -pedantic -Wall -g -lm
DESTDIR ?= /usr/local
TARGET ?= 3hstool

.PHONY: clean all install
all: $(TARGET)
clean:
	@rm -f $(OBJS) hlink

install: $(TARGET)
	install -m 557 $(TARGET) $(DESTDIR)/bin

%.o: %.c
	$(CC) -c $(<) -o $(@) $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) $(^) -o $(TARGET) $(CFLAGS)

