SRCS = src/*.c
OUT  = MladyFatFS
CC   = gcc
CCFLAGS = -Wall

all: clean build

build: $(SRCS)
	$(CC) $(SRCS) -o $(OUT) $(CCFLAGS)
	chmod 711 $(OUT)

clean: $(OUT)
	rm $(OUT)
