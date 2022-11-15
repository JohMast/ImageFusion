.onLoad <- function(libname, pkgname) {
  if(Sys.info()["sysname"]=="Windows"){
    path_to_proj <- file.path(Sys.getenv("R_LIBS_USER"),"Imagefusion/proj")
    Sys.setenv(PROJ_LIB=path_to_proj)
  }
}