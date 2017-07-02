```
   _
  (_)_ __   ___ _____   _
  | | '_ \ / _ \_  / | | |
  | | |_) |  __// /| |_| |
 _/ | .__/ \___/___|\__, |
|__/|_|             |___/
```
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/ae48c146f92e4fb088116091727440ad)](https://www.codacy.com/app/falgon/jpezy?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=falgon/jpezy&amp;utm_campaign=Badge_Grade)


## jpeg + lazy = jpezy

[![Codacy Badge](https://api.codacy.com/project/badge/Grade/ae48c146f92e4fb088116091727440ad)](https://www.codacy.com/app/falgon/jpezy?utm_source=github.com&utm_medium=referral&utm_content=falgon/jpezy&utm_campaign=badger)

The lazy and simply JPEG encoder implementation. This project is not for practical because it was implemented for learning.

## Requirements

* GCC 7.1.0 or later (Has not operation check, but this would probably work in the Clang version 4.0.0 or later,too.)
* [Boost C++ Libraries](http://www.boost.org/)
* [Srook C++ Libraries](https://github.com/falgon/SrookCppLibraries)

## Limitations and specifications

* Supported inputing file format is [PNM(Portable aNyMap) P3(portable pixmap format)](https://en.wikipedia.org/wiki/Netpbm_format) only. If You can use the ImageMagick, can [convert](https://www.imagemagick.org/script/convert.php) as like bellow.
```
convert -compress none <input image file> <output.ppm>
```
* Huffman codes and quantization tables are fixed (Reference: ISO/IEC 10918-1 ITU-T 81 Annex K)
* JFIF version 1.02
* The sampling rate is fixed to 2 : 1
* The restart interval is unabled
* The thumbnail is unabled
* Even if it is a gray scale image, this tool will use component that Cb and Cr

The following is an example of the result that compressed [The Standart Test Images of Lena](http://www.ece.rice.edu/~wakin/images/).
```
% convert -compress none lena512color.tiff lena.ppm
% ./jpezy lena.ppm output.jpg
   _
  (_)_ __   ___ _____   _
  | | '_ \ / _ \_  / | | |
  | | |_) |  __// /| |_| |
 _/ | .__/ \___/___|\__, |
|__/|_|             |___/       by roki

Reading the input file... width: 512 height: 512
Done! Processing time: 1.06(sec)
Start encoding and writing ...
Write JPEG Header ... Done! Processing time: 0(sec)
Encoding ... Done! Processing time: 0.78(sec)
Write EOI ... Done! Processing time: 0(sec)
Output size: 18031 byte
Done! Processing time: 0.825(sec)
Total processing time: 1.885

% file output.jpg
output.jpg: JPEG image data, JFIF standard 1.02, resolution (DPI), density 96x96, segment length 16, comment: "Encoded by jpezy", baseline, precision 8, 512x512, frames 3
```

## Usage
```cpp
Usage: jpezy <input.ppm> ( <ouput.(jpeg | jpg) [OPT: --gray]> | <output.ppm> | --debug )
```
* <i>input.ppm</i>: Input ppm image file
* <i>output.(jpeg | jpg) [OPT: --gray]</i>: Output jpeg or jpg image. When you designate the `--gray` option, the image is output in gray scale.
* <i>output.ppm</i>: When you designate ppm, the image is output ppm without doing anything.
* <i>--debug</i>: When you designate this option, the image is not output, and ppm data are just output on standard output.

## License
[MIT License](https://github.com/falgon/jpezy/blob/master/LICENSE)
