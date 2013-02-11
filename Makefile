LIBJIT_INCLUDE := -Ilibjit-install/include
LIBJIT_LIB := -Llibjit-install/lib

DEBUG_FLAGS = -ggdb -O0

SOURCES   := $(wildcard *.c)
OBJECTS   := $(patsubst %.c, %.o, $(wildcard *.c))

CFLAGS  += -I. $(DEBUG_FLAGS) -pthread $(LIBJIT_INCLUDE)
LDFLAGS += $(LIBJIT_LIB) -ljit -lm

all: jit_test

jit_test: $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o *~ core jit_test
