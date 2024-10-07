# How to Build

## Prerequisites

To build **mount-zip**, you need the following libraries:

*   [Boost Intrusive](https://www.boost.org)
*   [ICU](https://icu.unicode.org)
*   [libfuse >= 2.7](https://github.com/libfuse/libfuse)
*   [libzip >= 1.9.1](https://libzip.org)

On Debian systems, you can get these libraries by installing the following
packages:

```sh
$ sudo apt install libboost-container-dev libicu-dev libfuse-dev libzip-dev
```

To build **mount-zip**, you also need the following tools:

*   C++20 compiler (g++ or clang++)
*   [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
*   [GNU make](https://www.gnu.org/software/make/)
*   [Pandoc](https://pandoc.org) to generate the man page

On Debian systems, you can get these tools by installing the following packages:

```sh
$ sudo apt install g++ pkg-config make pandoc
```

To test **mount-zip**, you also need the following tools:

*   [Python >= 3.8](https://www.python.org)

On Debian systems, you can get these tools by installing the following packages:

```sh
$ sudo apt install python3
```

## Build **mount-zip**

```sh
$ make
```

## Test **mount-zip**

```sh
$ make check
```

## Install **mount-zip**

```sh
$ sudo make install
```

## Uninstall **mount-zip**

```sh
$ sudo make uninstall
```
