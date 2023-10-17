CC = gcc

TARGET = lc3-vmm

CFLAGES = -O2 -I.
LIBS = -lpthread

DIRS = .

FILES = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))

OBJS = $(patsubst %.c,%.o, $(FILES))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS) $(CFLAGES)

$(OBJS):%.o: %.c
	$(CC) -c $< -o $@ $(LIBS) $(CFLAGES)

test:
	./lc3-vmm ../lc3-vm/lc3-vm.obj

clean:
	$(RM) $(OBJS) $(TARGET)
