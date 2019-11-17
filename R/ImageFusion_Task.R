#' imagefusion_task
#'
#' @param ... 
#' @param filenames_high 
#' @param filenames_low 
#' @param dates_high 
#' @param dates_low 
#' @param dates_pred 
#' @param singlepair_mode 
#' @param method 
#' @param verbose 
#'
#' @return A ggplot overview of the tasks.
#' @import assertthat ggplot2 magrittr dplyr
#' @importFrom assertthat assert_that
#' @export
#'
#' @examples
#' 
#' 
#' 
imagefusion_task <- function(...,filenames_high,filenames_low,dates_high,dates_low,dates_pred,singlepair_mode="mixed",method="starfm",verbose=T,output_overview=T){

####1: Prepare Inputs####
filenames_pred <- paste("../../Pred_",dates_pred,".tif",sep = "")  #Make plausible out filenames

#Make sure that method is plausible
assert_that(method %in% c("estarfm","fitfc","spstfm","starfm"))

#Set singlepair policy
#ignore: Do not even attempt to predict singlepair dates
#mixed: Use doublepair mode when possible, and singlepair mode otherwise
#all: Predict all dates in singlepair mode


assert_that(singlepair_mode %in% c("ignore","mixed","all"))
#mixed and all are only possible in fitfc and starfm
assert_that(!(singlepair_mode =="mixed" & ( method == "estarfm" | method == "spstfm")))
assert_that(!(singlepair_mode =="all" & ( method == "estarfm" | method == "spstfm")))


#2DO DOES EVERYTHING ALSO WORK IF DATES DO NOT START AT 0?

####2: Find and prepare the jobs####
#Make one data frame for the high images and one for the low images
high_df <- data.frame(date = dates_high, has_high = rep(TRUE,length(filenames_high)), files_high = filenames_high,row.names = NULL,stringsAsFactors = F)
low_df <- data.frame(date = dates_low, has_low = rep(TRUE,length(filenames_low)), files_low = filenames_low,row.names = NULL,stringsAsFactors = F)
pred_df <- data.frame(date = dates_pred, predict= rep(TRUE,length(filenames_pred)), files_pred = filenames_pred,row.names = NULL,stringsAsFactors = F)

#Find special cases
pairs <- inner_join(x = high_df,y = low_df, by = "date")%>% arrange(date)
low_only <- anti_join(x = low_df,y = high_df, by = "date")%>% arrange(date)
low_outliers <- low_only[low_only$date>max(pairs$date) | low_only$date<min(pairs$date),]%>% arrange(date)

#join them into different tables #2DO: CHeck later, which of these are actually required
all <- full_join(x = high_df,y = low_df,by=c("date")) %>%
  full_join(y = pred_df,by=c("date")) %>% 
  arrange(date) %>% 
  mutate(date = as.numeric(date)) %>% 
  mutate(has_high = !is.na(has_high)) %>% 
  mutate(has_low = !is.na(has_low)) %>% 
  mutate(predict = !is.na(predict)) %>% 
  mutate(resolutions = factor(paste(has_high,has_low,sep = "-"),levels=c("FALSE-FALSE","FALSE-TRUE","TRUE-TRUE","TRUE-FALSE"))) %>% 
  mutate(pred_case = case_when(
    predict == FALSE ~ 0,                                      #No Prediction Requested -> No Prediction
    predict == TRUE & has_low == TRUE & has_high==FALSE & !date %in% low_outliers$date ~ 1,   #Only low available -> Normal Prediction
    predict == TRUE & has_low == TRUE & has_high==FALSE & date %in% low_outliers$date ~ 2,                         #Only low available + Outlier -> Prediction depends on ignore_outliers
    predict == TRUE & has_low == FALSE & has_high==FALSE ~ 3,  #Neither High nor low available -> Fail
    predict == TRUE & has_low == TRUE & has_high==TRUE ~ 4,    #Both High and Low available. -> Prediction Unnecessary
    predict == TRUE & has_low == FALSE & has_high==TRUE ~ 5    #Only High available -> Prediction impossible, but unnecessary
  )) %>% ungroup

if(singlepair_mode=="ignore"){
    all <- all %>% 
      mutate(interval_pairs = cut(date,breaks = c(-Inf,pairs$date,Inf),right = T,include.lowest = T)) %>% 
      mutate(interval_ids = cut(date,breaks = c(-Inf,pairs$date,Inf),labels = F,right = T,include.lowest = T)) %>% 
      mutate(interval_startpair = suppressWarnings(apply(cbind(lower = as.numeric( sub("\\((.+),.*", "\\1", interval_pairs) ),
                                          upper = as.numeric( sub("[^,]*,([^]]*)\\]", "\\1", interval_pairs))), 1, function(x) min(x[!(is.infinite(x)|is.na(x))])))) %>% 
      mutate(interval_endpair = suppressWarnings(apply(cbind(lower = as.numeric( sub("\\((.+),.*", "\\1", interval_pairs) ),
                                          upper = as.numeric( sub("[^,]*,([^]]*)\\]", "\\1", interval_pairs))), 1, function(x) max(x[!(is.infinite(x)|is.na(x))])))) %>% 
      group_by(interval_ids) %>%
      mutate(interval_start = min(date)-1) %>% 
      mutate(interval_end = max(date))%>% 
      mutate(interval_npred = sum(predict[pred_case==1],na.rm = T)) %>% 
      ungroup
  
}
if(singlepair_mode=="mixed"){
    all <- all %>% 
      mutate(interval_pairs = cut(date,breaks = c(-Inf,pairs$date,Inf),right = T,include.lowest = T)) %>% 
      mutate(interval_ids = cut(date,breaks = c(-Inf,pairs$date,Inf),labels = F,right = T,include.lowest = T)) %>%
      mutate(interval_startpair = suppressWarnings(apply(cbind(lower = as.numeric( sub("\\((.+),.*", "\\1", interval_pairs) ),
                                          upper = as.numeric( sub("[^,]*,([^]]*)\\]", "\\1", interval_pairs))), 1, function(x) min(x[!(is.infinite(x)|is.na(x))])))) %>% 
      mutate(interval_endpair = suppressWarnings(apply(cbind(lower = as.numeric( sub("\\((.+),.*", "\\1", interval_pairs) ),
                                          upper = as.numeric( sub("[^,]*,([^]]*)\\]", "\\1", interval_pairs))), 1, function(x) max(x[!(is.infinite(x)|is.na(x))])))) %>% 
      
      group_by(interval_ids) %>%
      mutate(interval_start = min(date)-1) %>% 
      mutate(interval_end = max(date))%>% 
      mutate(interval_npred = sum(predict[pred_case==1 | pred_case==2],na.rm = T)) %>% 
      ungroup
  
}
if(singlepair_mode=="all"){
  all <- all %>% 
    mutate(interval_pairs = as.factor(sapply(date, FUN = function(x) {pairs$date[which.min(abs(pairs$date-x))]}))) %>% 
    mutate(interval_ids = sapply(date, FUN = function(x) {which.min(abs(pairs$date-x))})) %>%
    mutate(interval_startpair = interval_pairs) %>% 
    mutate(interval_endpair = interval_pairs) %>% 
    group_by(interval_ids) %>%
    mutate(interval_start = min(date)-1) %>% 
    mutate(interval_end = max(date)) %>% 
    mutate(interval_npred = sum(predict[pred_case==1 | pred_case==2],na.rm = T)) %>% 
    ungroup
}


if(output_overview){
  #Output Task overview
  cat("\n=======================TASK OVERVIEW========================\n")
  case_1 <- all$date[all$pred_case==1]
  if(length(case_1)){cat(paste("Detected Dates between pairs:", paste(case_1,collapse = ",")))
    if(singlepair_mode=="all"){cat("\nPerforming Singlepair mode prediction for these dates.")}
    if(singlepair_mode=="mixed"){cat("\nPerforming Doublepair mode prediction for these dates.")}}
  case_2 <- all$date[all$pred_case==2]
  if(length(case_2)){cat(paste("Detected Dates outside of pairs:", paste(case_2,collapse = ",")))
    if(singlepair_mode=="ignore"){cat("\nIgnoring Singlepairs")}
    if(singlepair_mode %in% c("mixed","all")){cat("\nPerforming Singlepair mode prediction for these dates.")}}
  case_3 <- all$date[all$pred_case==3]
  if(length(case_3)){cat(paste("Cannot attempt prediction for dates:", paste(case_3,collapse = ","),"\nNo input images were found for those dates."))}
  case_4 <- all$date[all$pred_case==4]
  if(length(case_4)){cat(paste("Will not attempt prediction for dates", paste(case_4,collapse = ","),"\nHigh and low resolution input images were found for those dates, making prediction possible, but unnecessary."))}
  case_5 <- all$date[all$pred_case==5]
  if(length(case_5)){cat(paste("Will not attempt prediction for dates", paste(case_5,collapse = ","),"\nOnly a high resolution input image was found for those dates, making prediction impossible but unnecessary."))}
  cat("\n============================================================\n")
}

#Find jobs
  valid_job_table <- all %>% 
  group_by(interval_ids,interval_pairs,interval_start,interval_end,interval_npred,interval_startpair,interval_endpair) %>% 
  filter(sum(interval_npred)>0) %>% 
  summarise()

#Plot Overview
p <- ggplot(all, aes(x=date, y=resolutions,colour=resolutions,shape=predict,size=predict)) + 
  ggtitle("Task Overview")+
  scale_y_discrete(name="",limits=c("FALSE-FALSE","FALSE-TRUE","TRUE-TRUE","TRUE-FALSE"),labels=c("None","Low","Both \n(Pair)","High"))+
  scale_color_discrete(name="Available \nResolutions",labels=c("None","Low","Both \n(Pair)","High"))+
  scale_shape_manual(name="Predict \nDate?",values=c(1,15))+
  scale_size_manual(values=c(2,3),guide="none")+
  geom_rect(data=valid_job_table, aes(xmin=interval_start, xmax=interval_end, ymin=-Inf, ymax=+Inf),
            color=rainbow(length(valid_job_table$interval_end)),
            alpha=0.2,
            inherit.aes = FALSE)+
  geom_jitter(alpha=1,width=0,height=0.1) 



###3: Predictions (Cases 1 and 2) ####
for(i in 1:nrow(valid_job_table)){ #For every job
  #Get the pair dates
  current_job <- valid_job_table[i,]
  current_case_1 <- all[all$interval_ids==current_job$interval_ids & all$pred_case == 1,]
  current_case_2 <- all[all$interval_ids==current_job$interval_ids & all$pred_case == 2,]
  
  #Process Cases 1
  if(nrow(current_case_1)){
    startpair <- current_job$interval_startpair
    startpair_date <- all[all$date==startpair,]
    endpair <-  current_job$interval_endpair
    endpair_date <- all[all$date==endpair,]
    if(verbose){cat(paste("Job ",i,"predicting in doublepair mode for dates", paste(current_case_1$date,collapse = " ")," based on pairs on dates  ", startpair, endpair))}
    #ESTARFM
    if(method=="estarfm"){
      ImageFusion::estarfm_job(input_filenames = c(startpair_date$files_high,
                                                   startpair_date$files_low,
                                                   endpair_date$files_high,
                                                   endpair_date$files_low,
                                                   current_case_1$files_low),
                               input_resolutions = c("high","low","high","low",rep("low",nrow(current_case_1))),
                               input_dates = c(startpair,startpair,endpair,endpair,current_case_1$date),
                               pred_dates = current_case_1$date,
                               pred_filenames =  current_case_1$files_pred,verbose=verbose,...
      )
    }#end estarfm
    #FITFC
    if(method=="fitfc"){
      ImageFusion::fitfc_job(input_filenames = c(startpair_date$files_high,
                                                   startpair_date$files_low,
                                                   endpair_date$files_high,
                                                   endpair_date$files_low,
                                                   current_case_1$files_low),
                               input_resolutions = c("high","low","high","low",rep("low",nrow(current_case_1))),
                               input_dates = c(startpair,startpair,endpair,endpair,current_case_1$date),
                               pred_dates = current_case_1$date,
                               pred_filenames =  current_case_1$files_pred,verbose=verbose,...
      )
    }#end fitfc
    #STARFM
    if(method=="starfm"){
      ImageFusion::starfm_job(input_filenames = c(startpair_date$files_high,
                                                   startpair_date$files_low,
                                                   endpair_date$files_high,
                                                   endpair_date$files_low,
                                                   current_case_1$files_low),
                               input_resolutions = c("high","low","high","low",rep("low",nrow(current_case_1))),
                               input_dates = c(startpair,startpair,endpair,endpair,current_case_1$date),
                               pred_dates = current_case_1$date,
                               pred_filenames =  current_case_1$files_pred,verbose=verbose,...
      )
    }#end starfm
    #SPSTFM
    if(method=="spstfm"){
      ImageFusion::spstfmfm_job(input_filenames = c(startpair_date$files_high,
                                                   startpair_date$files_low,
                                                   endpair_date$files_high,
                                                   endpair_date$files_low,
                                                   current_case_1$files_low),
                               input_resolutions = c("high","low","high","low",rep("low",nrow(current_case_1))),
                               input_dates = c(startpair,startpair,endpair,endpair,current_case_1$date),
                               pred_dates = current_case_1$date,
                               pred_filenames =  current_case_1$files_pred,verbose=verbose,...
      )
    }#end spstfm
  }#end case1
  #Process Cases 2
  if(nrow(current_case_2)){
    
    startpair <- current_job$interval_startpair
    startpair_date <- all[all$date==startpair,]
    if(verbose){cat(paste("Job ",i,"predicting in singlepair mode for dates", paste(current_case_2$date,collapse = " ")," based on pair on date", startpair))}
    
    #FITFC
    if(method=="fitfc"){
      ImageFusion::fitfc_job(input_filenames = c(startpair_date$files_high,
                                                 startpair_date$files_low,
                                                 current_case_2$files_low),
                             input_resolutions = c("high","low",rep("low",nrow(current_case_2))),
                             input_dates = c(startpair,startpair,current_case_2$date),
                             pred_dates = current_case_2$date,
                             pred_filenames =  current_case_2$files_pred,verbose=verbose,...
      )
    }#end fitfc
    #STARFM
    if(method=="starfm"){
      ImageFusion::starfm_job(input_filenames = c(startpair_date$files_high,
                                                 startpair_date$files_low,
                                                 current_case_2$files_low),
                             input_resolutions = c("high","low",rep("low",nrow(current_case_2))),
                             input_dates = c(startpair,startpair,current_case_2$date),
                             pred_dates = current_case_2$date,
                             pred_filenames =  current_case_2$files_pred,verbose=verbose,...
      )
    }#end starfm
  }#end case2

} #End for every job

####4: Deal with other cases####

current_case_3 <- all[all$pred_case == 3,]
current_case_4 <- all[all$pred_case == 4,]
current_case_5 <- all[all$pred_case == 5,]

#Process Cases 3
if(nrow(current_case_3)){
  if(verbose){cat(paste("\nNo input images could be found for date(s)", paste(current_case_3$date,collapse = " "),". Skipping."))}
}#end case3
#Process Cases 4
if(nrow(current_case_4)){
  if(verbose){cat(paste("\nBoth high and low images were found for date(s)", paste(current_case_4$date,collapse = " "),". Skipping prediction and copying high input to target destination."))}
  file.copy(current_case_4$files_high,current_case_4$files_pred)
  }#end case4
#Process Cases 5
if(nrow(current_case_5)){
  if(verbose){cat(paste("\nOnly high images were found for date(s)", paste(current_case_5$date,collapse = " "),". Copying high input to target destination."))}
  file.copy(current_case_5$files_high,current_case_5$files_pred)
}#end case5

if(output_overview){return(p)}else{return(0)}

  
}#end function body
