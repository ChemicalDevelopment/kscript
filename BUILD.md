# Build Instructions

This document is a more in-depth guide to building kscript than is presented in the `README.md` file.

## On Unix/Linux/MacOS

Requirements:

  1. A C compiler that supports `C99`, although most compilers will work even if they don't fully support `C99`
  2. A `make`-based build system

Optional Dependencies:

  1. The Posix threading library (`pthread`)
    * If this is not present, then threading support is emulated
  2. The GNU Multiple Precision library (`gmp`)
    * If this is not present, then kscript will use an implementation of a subset of GMP routines. This means operations with large integers will be slower
  3. The GNU Readline Library (`readline`)
    * If this is not present, then a backup 'linenoise' implementation is used for line editing
  4. The Fastest Fourier Transform in the West (`fftw`)
    * If this is not present, then the implementation of FFT plans may be slower
  5. Libav (`libav`)
    * If this is not present, then `av` won't support nearly as many media formats


For example, you can install these on various platforms:

Debian/Ubuntu/etc.: `sudo apt install libpthread-stubs0-dev libgmp-dev libreadline-dev libfftw3-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev`

Now, once you have installed the dependencies you want, you can build the library via:

```bash
./configure # give it '--help' to display options
make
make check # runs the standard tests
make install # installs kscript to your system
```

### Further Customization

You can customize kscript via the configure script. For example, run `./configure -h` to show options:

```bash
$ ./configure -h
Usage: ./configure [options]
  -h,--help              Print this help/usage message and exit
  --prefix               Sets the prefix that the software should install to (default: /usr/local)
  --dest-dir             Destination locally to install to (but is not kept for runtime) (default: )
  --platform-name        Sets the platform name (default: auto)
  --shared-ext           Sets the extension for shared libs (default: auto) (e.g.: .so,.dll,.js)
  --static-ext           Sets the extension for static libs (default: auto) (e.g.: .a,.lib,.js)
  --binary-ext           Sets the extension for executables (default: auto) (e.g.: .exe,.js)
  --colors               Sets whether or not colors should be used in the interpreter (default: auto) (e.g.: on,off)

kscript configure script, any questions, comments, or concerns can be sent to:
Cade Brown <brown.cade@gmail.com>
```

(the output may be slightly different)

So, to install in a non-standard location (for example, locally), you could run:

```bash
$ ./configure --prefix $HOME/.local
``` 

And then `make` and so on to built it normally

## On Windows

See the `winbuild` dir for VisualStudio solutions/projects

## On Emscripten

kscript supports building for [emscripten](https://emscripten.org/), which allows kscript to be executed in the browser (or anywhere else where WASM can be ran). Currently, it is not recommended to build with threading support, as different browsers have differing support. To compile it, ensure you've installed emscripten, and run:

```shell
$ CC=emcc CFLAGS="-O3 -Wno-ignored-attributes -sASSERTIONS=1" LDFLAGS='-O3 --pre-js src/web/pre.js -sWASM=1 -sASSERTIONS=1 -sMODULARIZE=1 -sEXPORT_NAME="libks" -sERROR_ON_UNDEFINED_SYMBOLS=0 -sEXTRA_EXPORTED_RUNTIME_METHODS=[\"cwrap\",\"ccall\",\"stringToUTF8\",\"UTF8ToString\"]' PLATFORM="web" ./configure --with-pthreads off
$ make -j16 lib/libks.js
```

(note: Some shells may escape the content differently)

That should create the main library (`lib/libks.js`) which contains the relevant code, and can be loaded via `libks().then( ... )` function. See `src/web` for information on using it in the browser.

For threaded code, see: [https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer/Planned_changes](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer/Planned_changes)
