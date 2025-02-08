# CPU/Memory usage:

```
3 voices + reverb
memory usage: 33.1% (170036/513908)             
us per voice: 3.2                               
us per reverb: 4.6                              
% of block: 67.8%   
```

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
