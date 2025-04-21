ROSS_DIR = /gpfs/u/scratch/PCPF/PCPFkrpp/ross-install
CC = mpicc
CFLAGS = -I$(ROSS_DIR)/include -O2 -Wall -g
LDFLAGS = $(ROSS_DIR)/lib/libROSS.a -lm

all: plagueInc plague1lp

plagueInc: plagueInc.c plagueInc.h
	$(CC) $(CFLAGS) -o plagueInc plagueInc.c $(LDFLAGS)

plague1lp: plague1lp.c plague1lp.h
	$(CC) $(CFLAGS) -o plague1lp plague1lp.c $(LDFLAGS)

clean:
	rm -f plagueInc plague1lp