CC := g++
CFLAGS := -O3 -flto -Wall -pedantic
TARGETS = assignment fault overloading

.PHONY: all
all: $(TARGETS)

.PHONY: clean
clean:
	rm -f $(TARGETS)

%: %.cc
	$(CC) $(CFLAGS) $< -o $@
