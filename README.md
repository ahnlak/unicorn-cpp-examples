# Galactic Unicorn C(++) Examples

This is a collection of (hopefully informative!) examples for programming the
sublime Galactic Unicorn. This project is based on Pimoroni's helpful boilerplate,
so I've left the basic build instructions below.

Each example is a standalone single source file; CMake will produce a collection
of `uf2` files on building, so you can install whichever one you like to your
Unicorn.

## rain

A port of [my MicroPython version](https://github.com/ahnlak/unicorn-toys/blob/main/rain.py)
of the ancient `rain` text terminal demo; raindrops on falling on your Unicorn.


# Building

Please note that I don't use Visual Studio myself, so I can't promise I haven't
broken something in those builds. Stick to the command line for an easy life :-)

Other than that, it's a normal CMake-style build:

```
mkdir build
cd build
cmake -DWIFI_SSID="mynetwork" -DWIFI_PASSWORD="mypasswd" ..
make
```

This should generate a collection of `uf2` files, one for each example.


## Troubleshooting


## Before you start

It's easier if you make a `pico` directory or similar in which you keep the SDK, Pimoroni Libraries and your projects alongside each other. This makes it easier to include libraries.

## Preparing your build environment

Install build requirements:

```bash
sudo apt update
sudo apt install cmake gcc-arm-none-eabi build-essential
```

And the Pico SDK:

```
git clone https://github.com/raspberrypi/pico-sdk
cd pico-sdk
git submodule update --init
export PICO_SDK_PATH=`pwd`
cd ../
```

The `PICO_SDK_PATH` set above will only last the duration of your session.

You should should ensure your `PICO_SDK_PATH` environment variable is set by `~/.profile`:

```
export PICO_SDK_PATH="/path/to/pico-sdk"
```

## Grab the Pimoroni libraries

```
git clone https://github.com/pimoroni/pimoroni-pico
```

## Clone this repo

```
git clone https://github.com/ahnlak/unicorn-cpp-examples
cd unicorn-cpp-examples
```

If you have not or don't want to set `PICO_SDK_PATH` and you are using Visual Studio Code,
you can edit `.vscode/settings.json` to pass the path directly to CMake.

## Prepare Visual Studio Code

Open VS Code and hit `Ctrl+Shift+P`.

Type `Install` and select `Extensions: Install Extensions`.

Make sure you install:

1. C/C++
2. CMake
3. CMake Tools
4. Cortex-Debug (optional: for debugging via a Picoprobe or Pi GPIO)
5. Markdown All in One (recommended: for preparing your own README.md)
