# Kippenhok

Makes use of code from https://github.com/raspberrypi/pico-examples and https://github.com/raspberrypi/pico-extras to compile.


I had this folder positioned as
 ```
 - pico-examples
   - HERE
   - build
     - build.sh
   - ...
 - pico-extras
 - pico-sdk
 ```
 
 The build.sh fil contained
 
 ```bash
export PICO_SDK_PATH=../../pico-sdk
export PICO_EXTRAS_PATH=../../pico-extras
cmake ..
cd kippenhok
make -j4
 ```
 
 
