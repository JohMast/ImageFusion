if(getRversion() < "4.0.0") {
  stop("Your version of R is too old. This package requires R-4.0.0 or newer on Windows.")
}
VERSION <- "4.4.0"


# For details see: https://github.com/rwinlib/gdal2
# For details see: https://github.com/rwinlib/netcdf

if(!file.exists("../windows/gdal3-3.4.1/include/gdal.h")){
  download.file("https://github.com/rwinlib/gdal3/archive/v3.4.1.zip", "lib.zip", quiet = TRUE)
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


# if(!file.exists("../windows/opencv-%s/include/opencv4/opencv2/opencv.hpp")){
#   download.file("https://github.com/rwinlib/opencv/archive/v4.0.1.zip", "lib.zip", quiet = TRUE)
#   dir.create("../windows", showWarnings = FALSE)
#   unzip("lib.zip", exdir = "../windows")
#   unlink("lib.zip")
# }

if(!file.exists(sprintf("../windows/opencv-%s/include/opencv4/opencv2/opencv.hpp", VERSION))){
  download.file(sprintf("https://github.com/rwinlib/opencv/archive/v%s.zip", VERSION), "lib.zip", quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  unzip("lib.zip", exdir = "../windows")
  unlink("lib.zip")
}