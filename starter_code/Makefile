ROSS_DIR = /gpfs/u/scratch/PCPF/PCPFkrpp/ross-install
CC = mpicc
CFLAGS = -I$(ROSS_DIR)/include -O2 -Wall -g
LDFLAGS = $(ROSS_DIR)/lib/libROSS.a -lm


all: plagueInc 

plagueInc: plagueInc.c plagueInc.h
	$(CC) $(CFLAGS) -o plagueInc plagueInc.c $(LDFLAGS)

clean:
	rm -f plagueInc