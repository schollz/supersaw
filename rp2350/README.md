# Setup

```
git clone https://github.com/raspberrypi/openocd
cd openocd
./bootstrap
./configure
make -j8 
sudo make install
```


```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j8  && sudo openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 15000" -c "program hello_usb.elf verify reset exit"
```

```
> sudo openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 15000"

> gdb-multiarch yoctocore.elf  -ex "target remote localhost:3333" -ex "monitor reset init" -ex "continue"
```
