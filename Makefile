INSTALL_DIR=/usr/local/bin
LC3_DIR=lcc-1.3

all:
	@if [ ! -d $(INSTALL_DIR)/$(LC3_DIR) ]; then bash lcc/install.sh; fi

	make -C lc3-vm
	make -C lc3-vmm

clean:
	make -C lc3-vm clean
	make -C lc3-vmm clean

test:
	make -C lc3-vmm test

