library(dplyr)
library(ggplot2)
library(assertthat)
filenames_high <- list.files("../../Test/Landsat_MODIS_Fusion/Landsat_2015/",pattern = ".tif",full.names = T)
filenames_low <- list.files("../../Test/Landsat_MODIS_Fusion/MODIS_2015/",pattern = ".tif",full.names = T)
#drop a couple images for testing purposes
filenames_low <- filenames_low[-c(84,123,158)]

dates_high <- filenames_high %>%
  sapply(substr,64,71) %>%
  as.POSIXct(format="%Y%m%d") %>% 
  strftime(format = "%j") %>% 
  as.numeric()
dates_low <- filenames_low %>% 
  sapply(substr,52,54) %>% 
  as.numeric()

dates_pred <- c(50,51,100,107,123,125,126,128,155,158,200,201,202,280,281,282,283,355)

singlepair_mode <- "mixed"








####1: Prepare Inputs####
filenames_pred <- paste("Pred_",dates_pred,".tif",sep = "")  #Make plausible out filenames

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
#high_only <- anti_join(x = high_df,y = low_df, by = "date")%>% arrange(date)
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
    mutate(interval_pair1 = apply(cbind(lower = as.numeric( sub("\\((.+),.*", "\\1", interval_pairs) ),
                                        upper = as.numeric( sub("[^,]*,([^]]*)\\]", "\\1", interval_pairs))), 1, function(x) min(x[!(is.infinite(x)|is.na(x))]))) %>% 
    mutate(interval_pair3 = apply(cbind(lower = as.numeric( sub("\\((.+),.*", "\\1", interval_pairs) ),
                                        upper = as.numeric( sub("[^,]*,([^]]*)\\]", "\\1", interval_pairs))), 1, function(x) max(x[!(is.infinite(x)|is.na(x))]))) %>% 
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
    mutate(interval_pair1 = apply(cbind(lower = as.numeric( sub("\\((.+),.*", "\\1", interval_pairs) ),
                                        upper = as.numeric( sub("[^,]*,([^]]*)\\]", "\\1", interval_pairs))), 1, function(x) min(x[!(is.infinite(x)|is.na(x))]))) %>% 
    mutate(interval_pair3 = apply(cbind(lower = as.numeric( sub("\\((.+),.*", "\\1", interval_pairs) ),
                                        upper = as.numeric( sub("[^,]*,([^]]*)\\]", "\\1", interval_pairs))), 1, function(x) max(x[!(is.infinite(x)|is.na(x))]))) %>% 
    
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
    mutate(interval_pair1 = interval_pairs) %>% 
    mutate(interval_pair3 = interval_pairs) %>% 
    group_by(interval_ids) %>%
    mutate(interval_start = min(date)-1) %>% 
    mutate(interval_end = max(date)) %>% 
    mutate(interval_npred = sum(predict[pred_case==1 | pred_case==2],na.rm = T)) %>% 
    ungroup
}



#Output Task overview
print("=======================TASK OVERVIEW========================")
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
case_6 <- all$date[all$pred_case==6]
print("============================================================")


#Find jobs
if(singlepair_mode %in% c("mixed","ignore"))
valid_job_table <- all %>% 
  group_by(interval_ids,interval_pairs,interval_start,interval_end,interval_npred,interval_pair1,interval_pair3) %>% 
  filter(sum(interval_npred)>0) %>% 
  summarise()
#Find jobs
if(singlepair_mode == "all")
  valid_job_table <- all %>% 
  group_by(interval_ids,interval_pairs,interval_start,interval_end,interval_npred,interval_pair1,interval_pair3) %>% 
  filter(sum(interval_npred)>0) %>% 
  summarise()


#Plot Overview
ggplot(all, aes(x=date, y=resolutions,colour=resolutions,shape=predict,size=predict)) + 
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



####3: Predictions####
# 
# for(i in 1:nrow(valid_job_table)){ #For every job
#   #Get the pair dates
#   current_job <- valid_job_table[i,]
#   current_case_1 <- all[all$interval_ids==current_job$interval_ids & all$pred_case == 1,]
#   current_case_2 <- all[all$interval_ids==current_job$interval_ids & all$pred_case == 2,]
#   current_case_3 <- all[all$interval_ids==current_job$interval_ids & all$pred_case == 3,]
#   current_case_4 <- all[all$interval_ids==current_job$interval_ids & all$pred_case == 4,]
#   current_case_5 <- all[all$interval_ids==current_job$interval_ids & all$pred_case == 5,]
#   
#   
#   #Process Cases 1
#   if(nrow(current_case_1)){
#     start <- current_job$interval_start
#     startdate <- all[all$date==start,]
#     end <-  current_job$interval_end
#     enddate <- all[all$date==end,]
#     cat(paste("Job ",i,"predicting in doublepair mode for dates", paste(current_case_1$date,collapse = " ")," based on pairs on dates  ", valid_job_table$interval_pairs[i]))
#     #ESTARFM
#     if(method=="estarfm"){
#       ImageFusion::estarfm_job(input_filenames = c(startdate$files_high,
#                                                    startdate$files_low,
#                                                    enddate$files_high,
#                                                    enddate$files_low,
#                                                    current_case_1$files_low),
#                                input_resolutions = c("high","low","high","low",rep("low",nrow(current_case_1))),
#                                input_dates = c(start,start,end,end,current_case_1$date),
#                                pred_dates = current_case_1$date,
#                                pred_filenames =  current_case_1$files_pred,
#                                pred_area = c(400,    400, 100,  100),...
#       )
#     }
#     #FITFC
#     if(method=="fitfc"){
#       ImageFusion::fitfc_job(input_filenames = c(startdate$files_high,
#                                                  startdate$files_low,
#                                                  enddate$files_high,
#                                                  enddate$files_low,
#                                                  current_case_1$files_low),
#                              input_resolutions = c("high","low","high","low",rep("low",nrow(current_case_1))),
#                              input_dates = c(start,start,end,end,current_case_1$date),
#                              pred_dates = current_case_1$date,
#                              pred_filenames =  current_case_1$files_pred,
#                              pred_area = c(400,    400, 100,  100),...
#       )
#     }
#     #STARFM
#     if(method=="starfm"){
#       ImageFusion::starfm_job(input_filenames = c(startdate$files_high,
#                                                   startdate$files_low,
#                                                   enddate$files_high,
#                                                   enddate$files_low,
#                                                   current_case_1$files_low),
#                               input_resolutions = c("high","low","high","low",rep("low",nrow(current_case_1))),
#                               input_dates = c(start,start,end,end,current_case_1$date),
#                               pred_dates = current_case_1$date,
#                               pred_filenames =  current_case_1$files_pred,
#                               pred_area = c(400,    400, 100,  100),...
#       )
#     }
#     #SPSTFM
#     if(method=="spstfm"){
#       ImageFusion::spstfm_job(input_filenames = c(startdate$files_high,
#                                                   startdate$files_low,
#                                                   enddate$files_high,
#                                                   enddate$files_low,
#                                                   current_case_1$files_low),
#                               input_resolutions = c("high","low","high","low",rep("low",nrow(current_case_1))),
#                               input_dates = c(start,start,end,end,current_case_1$date),
#                               pred_dates = current_case_1$date,
#                               pred_filenames =  current_case_1$files_pred,
#                               pred_area = c(400,    400, 100,  100),...
#       )
#     }
#   }
#   #Process Cases 2
#   if(nrow(current_case_2)){
#     # Do a little regex to extract the appropriate date1 from the labels
#     # Note: This is a little complicated because of the mixed singlepair_mode, where
#     # we have interval breaks as if in doublepair mode,
#     # but possibly also have singlepair intervals (where one of the breaks is Inf)
#     # Here, we are extracting from the interval_pairs column a correct date1
#     pairs <- (cbind(lower = as.numeric( sub("\\((.+),.*", "\\1", current_job$interval_pairs) ),
#                  upper = as.numeric( sub("[^,]*,([^]]*)\\]", "\\1", current_job$interval_pairs) )))
#     
#     start <- max(pairs[!is.infinite(pairs)],na.rm = T)
#     
#     startdate <- all[all$date==start,]
#     cat(paste("Job ",i,"predicting in doublepair mode for dates", paste(current_case_2$date,collapse = " ")," based on pair on date", start))
#     
#   }
#   
#   
# } #End for every job
#   