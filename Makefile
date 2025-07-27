CC=gcc
CFLAGS=-g -Os
LIBS=-lSDL3
O=build

OBJS=	\
		$(O)/main.o \
		$(O)/display.o \
		$(O)/instructions.o \
		$(O)/insts_cb.o \
		$(O)/interrupts.o \

all:	$(O) $(O)/lcdboy

clean:
	rm ./build/*.o

$(O):
	mkdir -p $(O)

$(O)/lcdboy: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(O)/lcdboy $(LIBS)

$(O)/%.o: %.c | $(O)
	$(CC) $(CFLAGS) -c $< -o $@
