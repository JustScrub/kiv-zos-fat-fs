SRCS = src/*.c
OUT  = MladyFatFS
CC   = gcc

all: clean build

build: $(SRCS)
	$(CC) $(SRCS) -o $(OUT) -w
	chmod 711 $(OUT)

clean: $(OUT)
	rm $(OUT)
