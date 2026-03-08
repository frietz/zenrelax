CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lm
TARGET = zenrelax
SRC = zenrelax.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

debug: $(SRC)
	$(CC) -Wall -Wextra -g -O0 -o $(TARGET) $< $(LDFLAGS)

sanitize: $(SRC)
	$(CC) -Wall -Wextra -g -O1 -fsanitize=address,undefined -o $(TARGET) $< $(LDFLAGS)

format:
	clang-format -i $(SRC)

lint:
	cppcheck --enable=all --suppress=missingIncludeSystem $(SRC)

gifs: $(TARGET)
	@command -v agg > /dev/null || (echo "Install agg: download from https://github.com/asciinema/agg/releases" && exit 1)
	bash scripts/record-gifs.sh

clean:
	rm -f $(TARGET)

.PHONY: all debug sanitize format lint gifs clean
