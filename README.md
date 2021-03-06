```
   _
  (_)_ __   ___ _____   _
  | | '_ \ / _ \_  / | | |
  | | |_) |  __// /| |_| |
 _/ | .__/ \___/___|\__, |
|__/|_|             |___/
```
[![Build Status](https://travis-ci.org/falgon/jpezy.svg?branch=master)](https://travis-ci.org/falgon/jpezy)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/ae48c146f92e4fb088116091727440ad)](https://www.codacy.com/app/falgon/jpezy?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=falgon/jpezy&amp;utm_campaign=Badge_Grade)


## jpeg + lazy = jpezy

The lazy and simply JPEG encoder + JPEG decoder implementation. This project is not for practical because it was implemented for learning.

## Requirements

* GCC 7.1.0 or later (Has not operation check, but this would probably work in the Clang version 4.0.0 or later,too.)
* [Boost C++ Libraries](http://www.boost.org/)
* [Srook C++ Libraries](https://github.com/falgon/SrookCppLibraries)

## Limitations and specifications

### Encoder
* Supported inputing file format is [PNM(Portable anymap) P3(portable pixmap format)](https://en.wikipedia.org/wiki/Netpbm_format) only. If You can use the ImageMagick, can [convert](https://www.imagemagick.org/script/convert.php) as like bellow.
```
$ convert -compress none <input image file> <output.ppm>
```
* Huffman codes and quantization tables are fixed (Reference: ISO/IEC 10918-1 ITU-T 81 Annex K)
* JFIF version 1.02
* The sampling rate is fixed to 2 : 1
* The restart interval is unabled
* The thumbnail is unabled
* Even if it is a gray scale image, this tool will use component that Cb and Cr

The following is an example of the result that compressed [The Standart Test Images of Lena](http://www.ece.rice.edu/~wakin/images/).
```
$ convert -compress none lena512color.tiff lena.ppm
$ ./jpezy_encode lena.ppm lena.jpg
   _
  (_)_ __   ___ _____   _
  | | '_ \ / _ \_  / | | |
  | | |_) |  __// /| |_| |
 _/ | .__/ \___/___|\__, |
|__/|_|             |___/       by roki

Reading the input file... width: 512 height: 512
Done! Processing time: 0.522(sec)
Start encoding and writing ...
Write JPEG Header ... Done! Processing time: 0(sec)
Encoding ... Done! Processing time: 0.042(sec)
Write EOI ... Done! Processing time: 0(sec)
Output size: 18010 byte
Done! Processing time: 0.045(sec)
Total processing time: 0.567

$ file lena.jpg
output.jpg: JPEG image data, JFIF standard 1.02, resolution (DPI), density 96x96, segment length 16, comment: "Encoded by jpezy", baseline, precision 8, 512x512, frames 3
```

### Decoder
The following is an example of the result that compressed [The Standart Test Images of Lena](http://www.ece.rise.edu/~wakin/images).
```
$ ./jpezy_decode lena.jpg output.ppm
   _
  (_)_ __   ___ _____   _
  | | '_ \ / _ \_  / | | |
  | | |_) |  __// /| |_| |
 _/ | .__/ \___/___|\__, |
|__/|_|             |___/       by roki

process started...
     Loaded JPEG: 512x512, presicion 8, "Encoded by jpezy", JFIF standart 1.02, dots inch, frames 3, density 96x96

Done! Processing time: 0.055(sec)
Decoded image: Netpbm image data, size = 512 x 512, pixmap, ASCII text
$ file output.ppm
output.ppm: Netpbm image data, size = 512 x 512, pixmap, ASCII text
$ ./jpezy_decode lena.jpg output.ppm -v
   _
  (_)_ __   ___ _____   _
  | | '_ \ / _ \_  / | | |
  | | |_) |  __// /| |_| |
 _/ | .__/ \___/___|\__, |
|__/|_|             |___/       by roki

process started...
     analyzing header...
             found marker: [APP0]
                     analyzing jfif... Done! Processing time: 0(sec)
             found marker: [COM]
             found marker: [DQT]
                     analyzing DQT... Done! Processing time: 0(sec)
             found marker: [DQT]
                     analyzing DQT... Done! Processing time: 0(sec)
             found marker: [DHT]
                     analyzing DHT...  size: 12
found DC Huffman Table...                       Done! Processing time: 0(sec)
             found marker: [DHT]
                     analyzing DHT...  size: 12
found DC Huffman Table...                       Done! Processing time: 0(sec)
             found marker: [DHT]
                     analyzing DHT...  size: 162
found AC Huffman Table...                       Done! Processing time: 0(sec)
             found marker: [DHT]
                     analyzing DHT...  size: 162
found AC Huffman Table...                       Done! Processing time: 0(sec)
             found marker: [SOF0]
                     analyzing frames... VSize: 512 HSize: 512 Done! Processing time: 0(sec)
             found marker: [SOS]
                     analyzing scan data... Done! Processing time: 0(sec)
     Done! Processing time: 0(sec)
     Loaded JPEG: 512x512, presicion 8, "Encoded by jpezy", JFIF standart 1.02, dots inch, frames 3, density 96x96

     decoding started...     Done! Processing time: 0.052(sec)
Done! Processing time: 0.052(sec)
Decoded image: Netpbm image data, size = 512 x 512, pixmap, ASCII text
```

## Build
```sh
$ git clone https://github.com/falgon/jpezy.git
$ cd jpezy
$ mkdir build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make -j$(nproc)
```
or with [Ninja](https://ninja-build.org/)
```sh
$ git clone https://github.com/falgon/jpezy.git
$ cd jpezy
$ mkdir build && cd build
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
$ ninja
```

## Usage

### Encoder
```cpp
Usage: jpezy_encoder <input.ppm> ( <ouput.(jpeg | jpg) [OPT: --gray]> | <output.ppm> | --debug )
```
* <i>input.ppm</i>: Input ppm image file
* <i>output.(jpeg | jpg) [OPT: --gray]</i>: Output jpeg or jpg image. When you designate the `--gray` option, the image is output in gray scale.
* <i>output.ppm</i>: When you designate ppm, the image is output ppm without doing anything.
* <i>--debug</i>: When you designate this option, the image is not output, and ppm data are just output on standard output.

### Decoder
```cpp
Usage: jpezy_decoder <input.(jpg | jpeg)> ( <output.ppm | [OPT: --gray]> | -v )
```
* <i>input.(jpg | jpeg)</i>: Input JPEG image file
* <i>output.ppm</i>: Output ppm image file
* <i> --gray</i>: Image is output in gray scale.
* <i>-v</i>: Verbose mode.

The examples using decoder as a library are as follows.
```cpp
#include"jpezy_decoder.hpp"
#include"decode_io.hpp"

int main()
{
    jpezy::decoder<jpezy::Release> dec("input.jpg"); // appoint the input JPEG image file
    const auto raw = dec.decode<jpezy::COLOR_MODE>(); // Decoding and returning rgb raw data. This value wrapped optional.

    if (!raw) return EXIT_FAILURE;
    const auto& [r, g, b] = raw.value(); // getting rgb data by structure bindings. 

    std::ofstream ofs("output.ppm"); // appoint the output PPM image file.
    jpezy::decode_io decio(dec.pr.get<jpezy::property::At::HSize>(), dec.pr.get<jpezy::property::At::VSize>(), r, g, b); // passing data to helper that jpezy::decode_io
    ofs << decio; // Outputing the PPM file.
}
```

## Reference
* [ITU-T81](https://www.w3.org/Graphics/JPEG/itu-t81.pdf)
* [Independent JPEG Group's JPEG software の使い方](https://cetus.sakura.ne.jp/softlab/software2/jpeg6b_usage_j.html)
* [JPG ファイルフォーマット](http://www.setsuki.com/hsp/ext/jpg.htm)
* [JPEG メタデータについて (バイナリを眺めながら)](http://diary.awm.jp/~yoya/data/2015/10/16/JPEGMeta_rev3.pdf)
* [JPEG-概念からC++での実装まで](https://www.amazon.co.jp/JPEG%E2%80%95%E6%A6%82%E5%BF%B5%E3%81%8B%E3%82%89C-%E3%81%A7%E3%81%AE%E5%AE%9F%E8%A3%85%E3%81%BE%E3%81%A7-%E6%A9%8B%E6%9C%AC-%E6%99%8B%E4%B9%8B%E4%BB%8B/dp/4797330457)

## License
[MIT License](https://github.com/falgon/jpezy/blob/master/LICENSE)
