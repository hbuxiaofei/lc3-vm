LCC_PATH = /usr/local/bin/lcc-1.3/install

CC = lcc

TARGET = lc3-vm.obj

FILES = main.c

all: $(TARGET)

$(TARGET): $(FILES)
	PATH=$PATH:${LCC_PATH} $(CC) $(FILES) -o $(TARGET)

clean:
	$(RM) $(TARGET) lc3-vm.asm  lc3-vm.sym
