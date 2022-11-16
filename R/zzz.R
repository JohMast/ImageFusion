.onLoad <- function(libname, pkgname) {
  # if(Sys.info()["sysname"]=="Windows"){
  #   path_to_proj <- file.path(.libPaths(),"Imagefusion","proj")
  #   Sys.setenv(PROJ_LIB=path_to_proj)
  # }
  
  if (dir.exists(system.file("proj", package="gdalcubes"))) {
    Sys.setenv("PROJ_LIB" = system.file("proj", package="gdalcubes"))
  }
  if (dir.exists(system.file("gdal", package="gdalcubes"))) {
    Sys.setenv("GDAL_DATA" = system.file("gdal", package="gdalcubes"))
  }
  
}