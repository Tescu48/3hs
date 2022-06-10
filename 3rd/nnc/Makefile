
SOURCES  := source/read-stream.c source/exefs.c source/internal.c source/crypto.c source/sigcert.c source/tmd.c source/u128.c source/utf.c source/smdh.c source/romfs.c source/ncch.c source/exheader.c source/cia.c source/ticket.c
CFLAGS   ?= -ggdb3 -Wall -Wextra -pedantic
TARGET   := libnnc.a
BUILD    := build
LIBS     := -lmbedcrypto

TEST_SOURCES  := test/main.c test/extract-exefs.c test/tmd-info.c test/u128.c test/smdh.c test/romfs.c test/ncch.c test/exheader.c test/cia.c test/tik.c
TEST_TARGET   := nnc-test
LDFLAGS       ?=

# ====================================================================== #

TEST_OBJECTS := $(foreach source,$(TEST_SOURCES),$(BUILD)/$(source:.c=.o))
OBJECTS      := $(foreach source,$(SOURCES),$(BUILD)/$(source:.c=.o))
SO_TARGET    := $(TARGET:.a=.so)
DEPS         := $(OBJECTS:.o=.d)
CFLAGS       += -Iinclude -std=c99


.PHONY: all clean test shared run-test docs examples
all: $(TARGET)
run-test: test
	./$(TEST_TARGET)
docs:
	doxygen
test: $(TARGET) $(TEST_TARGET)
shared: CFLAGS += -fPIC
shared: clean $(SO_TARGET) # Clean because all objects need to be built with -fPIC
examples: bin/ bin/gm9_filename
clean:
	rm -rf $(BUILD) $(TARGET) $(SO_TARGET)

-include $(DEPS)

$(TEST_TARGET): $(TEST_OBJECTS) $(TARGET)
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS)

$(SO_TARGET): $(OBJECTS)
	$(CC) -shared $^ -o $@ $(LDFLAGS) $(LIBS)

$(TARGET): $(OBJECTS)
	$(AR) -rcs $@ $^

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS) $(LIBS) -MMD -MF $(@:.o=.d)

bin/:
	@mkdir -p bin

bin/gm9_filename: examples/gm9_filename.c $(TARGET)
	$(CC) $^ -o $@ $(LDFLAGS) $(CFLAGS) $(LIBS)

