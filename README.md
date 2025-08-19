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


## CI Pipelines Usage

There are two main workflows:

1. **Docker Publish Workflow** (`.github/workflows/docker-publish.yml`):  
  - This workflow builds the Docker image (`myndfirmware:latest`) and publishes it to the repository owner's GitHub Container Registry (GHCR).  
    - **You must run this workflow manually** whenever you update the Dockerfile or before running the build workflow for the first time.  
    - This ensures the latest build environment is available for building the Mynd firmware.

2. **Build Workflow** (`.github/workflows/ci.yml`):  
  - This workflow runs automatically on pushes, pull requests, or can be triggered manually.  
    - It pulls the `myndfirmware:latest` image from GHCR to build the firmware and bootloader.  
      - **If the Docker image has not been published yet, the build workflow will fail.**  
    - Always make sure the Docker Publish workflow has completed successfully before running or relying on the build workflow.

**Summary:**  
- Run the Docker Publish workflow first to build and push the Docker image.
- Then, run the Build workflow to build and upload the firmware artifacts.
