CFLAGS := -fPIC -O3 -g -Wall -Werror
CC := gcc

all: lrflisten

lrflisten: lrflisten.c
	$(CC) lrflisten.c -o $@ -L.  -lm -lwiringPi -mfloat-abi=hard -mfpu=vfp

clean:
	$(RM) lrflisten *.o *.so*
