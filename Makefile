CFLAGS+=-Wall -Werror
TARGET=snappy-fox

.PHONY: all
all: $(TARGET)

.PHONY: clear
clean:
	rm -f $(TARGET)
