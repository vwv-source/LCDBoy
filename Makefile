CC=gcc
CFLAGS=-g -Os
LIBS=-lSDL2
O=build

OBJS=	\
		$(O)/main.o \
		$(O)/display.o \
		$(O)/instructions.o \
		$(O)/insts_cb.o \

all:	$(O) $(O)/emain

clean:
	rm ./build/*.o

$(O):
	mkdir -p $(O)

$(O)/emain: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(O)/emain $(LIBS)

$(O)/%.o: %.c | $(O)
	$(CC) $(CFLAGS) -c $< -o $@
