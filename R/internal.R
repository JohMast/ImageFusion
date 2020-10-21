
self_predict <- function(...,src_im,dst_im,method){
  if(method=="fitfc"){
  ImageFusion::fitfc_job(input_filenames = rep(src_im,3),
                         input_resolutions = c("high","low","low"),
                         input_dates = c(1,1,1),
                         pred_dates = 1,
                         pred_filenames =  dst_im,
                         verbose=FALSE,
                         ...
                         )
  }
  if(method=="spstfm"){
  ImageFusion::spstfm_job(input_filenames = rep(src_im,5),
                         input_resolutions = c("high","low","high","low","low"),
                         input_dates = c(1,1,2,2,1),
                         pred_dates = 1,
                         pred_filenames =  dst_im,
                         verbose=FALSE,
                         min_train_iter = 0,
                         max_train_iter = 0,
                         LOADDICT_options = "",
                         SAVEDICT_options = "",
                         REUSE_options = "clear",
                         ...
  )
  }
  if(method=="estarfm"){
  ImageFusion::estarfm_job(input_filenames = rep(src_im,5),
                           input_resolutions = c("high","low","high","low","low"),
                           input_dates = c(1,1,2,2,1),
                         pred_dates = 1,
                         pred_filenames =  dst_im,
                         verbose=FALSE,
                         ...
  )
  }
  if(method=="starfm"){
  ImageFusion::starfm_job(input_filenames = rep(src_im,3),
                         input_resolutions = c("high","low","low"),
                         input_dates = c(1,1,1),
                         pred_dates = 1,
                         pred_filenames =  dst_im,
                         verbose=FALSE,
                         ...
  )
  }
  
  
  
  
  
}
