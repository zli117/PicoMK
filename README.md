# PicoMK <!-- omit in toc -->

PicoMK is a highly configurable mechanical keyboard firmware designed for Raspberry Pi Foundation's [RP2040](https://www.raspberrypi.com/documentation/microcontrollers/rp2040.html). The chip features two Cortex-M0 processors. Based on the smp branch of [FreeRTOS](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/smp), PicoMK supports multicore execution on RP2040. It's originally built for [Pico-Keyboard](https://github.com/zli117/Pico-Keyboard), but it can be easily adapted to other RP2040 based keyboards.

## Table of Contents

- [Features](#features)
- [Quick Start](#quick-start)
  - [Get the Code](#get-the-code)
  - [Build a Firmware](#build-a-firmware)
- [Documentations](#documentations)
  - [Anatomy of a layout.cc File](#anatomy-of-a-layoutcc-file)
- [Example Configurations](#example-configurations)
- [Future Roadmaps](#future-roadmaps)

# Features

* Configurable keymaps with multiple layers and compile time validation.
* Full [NKRO](https://en.wikipedia.org/wiki/Rollover_(keyboard)) support.
* C++ registration based customization.
* Supports multiple peripherals such as rotary encoder, SSD1306 OLED screen, joysticks, WS2812 LED and more to come.
* Runtime configuration menu (screen required). Change keyboard configuration on-the-fly without any host software.
  
  ![Config Menu Demo](docs/config_menu.gif)

# Quick Start

## Get the Code
This guide assumes you're using one of the followings: Linux (including Raspbian), Windows WSL, MacOS.

First, install the dependencies for pico-sdk

 * Arch Linux:

   ```bash
   pacman -S git cmake arm-none-eabi-gcc
   ```

 * Ubuntu:

   ```bash
   sudo apt update
   sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential libstdc++-arm-none-eabi-newlib
   ```

 * Raspbian:

   Note the installation script requires ~2.5GB of disk space on the SD card. 

   ```bash
   sudo apt install wget
   wget https://raw.githubusercontent.com/raspberrypi/pico-setup/master/pico_setup.sh
   chmod +x pico_setup.sh
   ./pico_setup.sh
   ```

   For details on what this script does, please see Pi Pico [Getting started guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) chapter 1.

 * MacOS:

   ```bash
   # Install Homebrew
   /bin/bash -c "$(curl -fsSL
   https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"
   
   brew install cmake
   brew tap ArmMbed/homebrew-formulae
   brew install arm-none-eabi-gcc
   
   # For M1 Mac only: install Rosetta 2
   /usr/sbin/softwareupdate --install-rosetta --agree-to-license
   ```

Then checkout the code

```bash
git clone https://github.com/zli117/PicoMK.git
cd PicoMK
git submodule update --init --recursive
```
The last command will checkout all the dependencies such as pico-sdk and FreeRTOS, so it might take a while depending on your internet connection.

## Build a Firmware

To create a custom firmware, you can make a copy of the existing config in the `configs/` folder. For this guide, we will copy the default config in `configs/default`. 

```bash
mkdir -p configs/tutorial/my_new_config
cp configs/default/* configs/tutorial/my_new_config
```

Each config consists of two files: `config.h` and `layout.cc`. Please see the comments in the file and the documentations for information on how to configure them.

The following commands builds the firmware:

```bash
mkdir build
cd build
cmake -DBOARD_CONFIG=<board_config> ..
make -j 4
```

`<board_config>` is the relative path of the custom config folder we have just created w.r.t the `configs/` folder. In our case, to build the `configs/tutorial/my_new_config` config, you can use this command: 

```bash
cmake -DBOARD_CONFIG=tutorial/my_new_config ..
```

Once you successfully build the firmware, you can find the `firmware.uf2` file under the current (`build/`) folder. Now take the Pico board (or other RP2040 boards you have) and put it into the bootloader mode (for Pico board, you can just hold down the bootsel button and replug the USB cable). Mount the board as USB mass storage device, if not done automatically. Copy over the `firmware.uf2` to the storage device folder and you're all set.

Please take a look at the following documentations on how to customize different parts of the firmware, including implementing your own custom keycode handler and more. 

# Documentations

## [Anatomy of a layout.cc File](docs/layout_cc.md)

Basic intro to how to configure a keyboard layout using `layout.cc` file. 

# Example Configurations

| Name                                                     | Info |
| -------------------------------------------------------- | ---- |
| [`examples/home_screen`](configs/examples/home_screen) | An example of customizing the home screen.
| [`examples/bare_minimum`](configs/examples/bare_minimum) | Show that registration is like conditional compilation. If you don't register something, it'll be stripped from the binary. `default` config binary size: 509440, `examples/bare_minimum` binary size: 372736 bytes

# Future Roadmaps

 * USB mass storage mode for importing and exporting json config file.
 * Support charlieplexing to save pins for other things such as driving an LED matrix.
 * Keyboard as a wifi dongle with Pico W (Nice for RPi2 and lower but is this too crazy?)
 * More peripherals such as SK6805, bigger screen, LED matrix, trackpad, etc.
 * USB hub
 * A way for people to check in their configs like in [QMK](https://github.com/qmk/qmk_firmware) 
