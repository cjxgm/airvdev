.PHONY: all clean
all: build/airv-send build/airv-recv
clean:
	rm -rf build/

build/%: src/%.c | build/
	$(CC) $(CFLAGS) -std=gnu11 -O3 -Wall -o $@ $<

.PRECIOUS: %/
%/:
	mkdir -p $@

