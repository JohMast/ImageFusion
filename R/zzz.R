.onLoad <- function(libname, pkgname){
  if(!ROpenCVLite::isOpenCVInstalled()){
    cat("OpenCV not installed, installing OpenCV")
    ROpenCVLite::installOpenCV()
    1
  }else{
    cat("OpenCV installed, all good!")
  }
}