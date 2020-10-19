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

checking compiled code ... NOTE
  File ‘ImageFusion/libs/ImageFusion.so’:
    Found ‘_ZSt4cerr’, possibly from ‘std::cerr’ (C++)
      Objects: ‘execture_imagefusor_jobs.o’,
        ‘utils/helpers/utils_common.o’, ‘src/fitfc.o’, ‘src/geoinfo.o’,
        ‘src/image.o’, ‘src/optionparser.o’, ‘src/spstfm.o’,
        ‘src/spstfm_impl.o’
    Found ‘_ZSt4cout’, possibly from ‘std::cout’ (C++)
      Objects: ‘execture_imagefusor_jobs.o’, ‘execture_imginterp_job.o’,
        ‘utils/helpers/utils_common.o’, ‘src/image.o’, ‘src/spstfm.o’,
        ‘src/spstfm_impl.o’, ‘src/staarch.o’
  
  
## Downstream dependencies

There are no downstreams dependencies yet, although our intent is to produce some in the future.