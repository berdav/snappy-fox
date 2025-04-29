CFLAGS+=-Wall -Werror -DVERSION='"v0.3.0"'
TARGET=snappy-fox

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -f $(TARGET)
