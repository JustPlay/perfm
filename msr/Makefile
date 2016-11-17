#
# Makefile for MSRs
#

CC       = g++ 
CFLAGS   =-std=c++11 -Wall -g -O2 -fomit-frame-pointer -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
LDFLAGS  = 

BIN	= msr_write  msr_read

sbindir = /usr/sbin

all: $(BIN)

clean:
	rm -f *.o $(BIN)

distclean: clean
	rm -f *~ \#*

install: all
	install -m 755 $(BIN) $(sbindir)

.o:
	$(CC) $(LDFLAGS) -o $@ $<

.c.o:
	$(CC) $(CFLAGS) -o $@ $<

.c:
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
