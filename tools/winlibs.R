if(getRversion() < "4.0.0") {
  stop("Your version of R is too old. This package requires R-4.0.0 or newer on Windows.")
}
VERSION <- commandArgs(TRUE)


# For details see: https://github.com/rwinlib/gdal2
# For details see: https://github.com/rwinlib/netcdf

if(!file.exists("../windows/gdal2-2.2.3/include/gdal/gdal.h")){
  download.file("https://github.com/rwinlib/gdal2/archive/v2.2.3.zip", "lib.zip", quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  unzip("lib.zip", exdir = "../windows")
  unlink("lib.zip")
}

if(!file.exists("../windows/netcdf-4.4.1.1-dap/include/netcdf.h")){
  download.file("https://github.com/rwinlib/netcdf/archive/v4.4.1.1-dap.zip", "lib.zip", quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  unzip("lib.zip", exdir = "../windows")
  unlink("lib.zip")
}


if(!file.exists("../windows/opencv-%s/include/opencv4/opencv2/opencv.hpp")){
  download.file("https://github.com/rwinlib/opencv/archive/v4.0.1.zip", "lib.zip", quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  unzip("lib.zip", exdir = "../windows")
  unlink("lib.zip")
}