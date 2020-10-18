## Test environments
* local windows 10, R 4.0.2
* local ubuntu 20.04, R 4.0.2
* win-builder (devel and release)

## R CMD check results
There were no ERRORs or WARNINGs. 

checking installed package size ... NOTE
    installed size is 103.6Mb
    sub-directories of 1Mb or more:
      gdal    3.8Mb
      libs   83.1Mb
      modis   1.4Mb
      proj    5.0Mb
      share   9.7Mb
      
This is mostly due to our inclusion of the gdal library, which is necessary for our handing of images.
Other packages handling raster files, such as sf or gdalcubes, also suffer from 
this. We hope to solve this problem and provide a suitable alternative, however,
have not been successful so far.

    
checking compiled code ... NOTE
  Warnung in read_symbols_from_dll(so, rarch)
    this requires 'objdump.exe' to be on the PATH
  Warnung in read_symbols_from_dll(so, rarch)
    this requires 'objdump.exe' to be on the PATH
  Warnung in read_symbols_from_dll(so, rarch)
    this requires 'objdump.exe' to be on the PATH
  Warnung in read_symbols_from_dll(so, rarch)
    this requires 'objdump.exe' to be on the PATH
  Warnung in read_symbols_from_dll(so, rarch)
    this requires 'objdump.exe' to be on the PATH
  Warnung in read_symbols_from_dll(so, rarch)
    this requires 'objdump.exe' to be on the PATH
  File 'ImageFusion/libs/i386/ImageFusion.dll':
    Found no calls to: 'R_registerRoutines', 'R_useDynamicSymbols'
  File 'ImageFusion/libs/x64/ImageFusion.dll':
    Found no calls to: 'R_registerRoutines', 'R_useDynamicSymbols'

This is presumably a local issue with our compiler.


## Downstream dependencies

There are no downstreams dependencies yet, although our intent is to produce some in the future.