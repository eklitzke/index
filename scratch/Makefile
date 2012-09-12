CC := g++
CFLAGS := -O3 -std=c++11 -flto -Wall -pedantic
TARGETS = assignment fault frozen_map overloading

.PHONY: all
all: $(TARGETS)

.PHONY: clean
clean:
	rm -f $(TARGETS)

%: %.cc
	$(CC) $(CFLAGS) $< -o $@
