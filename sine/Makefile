upload: build
	cd build && make -j8
	cd build && sudo openocd -f interface/cmsis-dap.cfg \
							-f target/rp2350.cfg \
							-c "adapter speed 15000" \
							-c "program hello_usb.elf verify reset exit"

clean:
	rm -rf build

build: pico-sdk pico-extras
	mkdir -p build
	cd build && cmake -DCMAKE_BUILD_TYPE=Debug ..

pico-sdk:
	git clone https://github.com/raspberrypi/pico-sdk
	cd pico-sdk && git submodule update --init --recursive

pico-extras:
	git clone https://github.com/raspberrypi/pico-extras
	cd pico-extras && git submodule update --init