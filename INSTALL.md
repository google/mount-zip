# How to Build

## Prerequisites

To build **mount-zip**, you need the following libraries:

*   [Boost Intrusive](https://www.boost.org)
*   [ICU](https://icu.unicode.org)
*   [libfuse >= 3.1](https://github.com/libfuse/libfuse)
*   [libzip >= 1.9.1](https://libzip.org)

On Debian systems, you can get these libraries by installing the following
packages:

```sh
$ sudo apt install libboost-container-dev libicu-dev libfuse3-dev libzip-dev
```

For compatibility reasons, **mount-zip** can optionally use the old FUSE 2
library [libfuse >= 2.9](https://github.com/libfuse/libfuse). On Debian systems,
you can install FUSE 2 by installing the following package:

```sh
$ sudo apt install libfuse-dev
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

### With debugging assertions

```sh
$ DEBUG=1 make
```

### With FUSE 2

```sh
$ FUSE_MAJOR_VERSION=2 make
```

## Test **mount-zip**

### All tests (including slow tests)

```sh
$ make check
```

### Only fast tests

```sh
$ make check-fast
```

## Install **mount-zip**

```sh
$ sudo make install
```

## Uninstall **mount-zip**

```sh
$ sudo make uninstall
```
