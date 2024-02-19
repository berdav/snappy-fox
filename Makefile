CFLAGS+=-Wall -Werror -DVERSION='"v0.3.0"'
TARGET=snappy-fox

.PHONY: all
all: $(TARGET)

.PHONY: clear
clean:
	rm -f $(TARGET)
