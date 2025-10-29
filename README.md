# Clone
```shell
git clone --recurse-submodules git@github.com:teufelaudio/mynd-firmware.git
```
# Build
```shell
mkdir build && cd build
cmake -DPROJECT=Mynd -DTOOLCHAIN_PREFIX=<path to compiler> -DCMAKE_BUILD_TYPE=Release ..
make mynd-update-firmware-mcu
```
# Project structure
 - `drivers`            - Basic stm I/O drivers
 - `firmwares`          - Third-party firmwares (Bluetooth, USB-PD controller)
 - `gitversion`         - Version string generator
 - `Projects`
   - `Mynd`             - Source code for MCU firmware
   - `MyndBootloader`   - Source code for bootloader
 - `sdk`                - Board support package
 - `support`
   - `cmake`            - Cmake helpers for stm32 components and other libraries
   - `docker-build`     - Configuration for docker build
   - `keys`             - Keys to sign firmware update file (Only for tests! They are not used.)
   - `scripts`          - Helper scripts for build and CI/CD
   - `svd`              - Hardware description file - internal registers outline


## Want to easily build and test MYND firmware using GitHub Actions?
View the [Wiki](https://github.com/teufelaudio/mynd-firmware/wiki) for more details!

## Don't Yet Own a MYND Speaker!?
Purchase one [here](https://teufel.de/mynd)!