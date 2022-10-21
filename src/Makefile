SRCS = commands.c fat_manager.c main.c
OUT  = MladyFatFS
CC   = gcc

all: clean build

build: $(SRCS)
	$(CC) $(SRCS) -o $(OUT)
	chmod 711 $(OUT)

clean: $(OUT)
	rm $(OUT)