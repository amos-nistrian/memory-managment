#
#comments --here--
#
#

CC = gcc
CFLAGS = -Wall -g -fmax-errors=2 #-fmax-errors=2 sets the max errors to be displayed to the first 2
LDFLAGS = -g -lm
OBJECTS = firstfit.o HeapTestEngine.o
EXES = HeapTest

HeapTest:	$(OBJECTS)
	$(CC) -o HeapTest $(LDFLAGS) $(OBJECTS)


all:
	$(MAKE) $(EXES)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f firstfit.o $(EXES)
