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
   - `keys`             - Keys to sign firmware update file (Only for tests! They are not used)
   - `scripts`          - Helper scripts for build and CI/CD
   - `svd`              - Hardware description file - internal registers outline


# Building MYND Firmware with GitHub Actions

Want to easily build MYND firmware and test your changes on your personal speaker?

- Follow the step-by-step instructions in this [How To guide](./Projects/Mynd/docs/how_to/build_mynd_with_actions.md)

# MCU Firmwares

- Learn more [here](./Projects/Mynd/docs/wiki/mcu_firmwares.md) about the different firmwares built for MYND
  
# For the tech-savvy

- If you wish to experiment with the firmware CLI, take a look [here](./Projects/Mynd/docs/how_to/use_tshell_cli.md)