
clean:
	make -C orb/app/boards/stm32f3discovery clean
	make -C orb/app/boards/orb_v1 clean

build_orb_discovery:
	make -C orb/app/boards/stm32f3discovery app_debug

build_orb:
	make -C orb/app/boards/orb_v1 app_debug

all: build_orb build_orb_discovery
