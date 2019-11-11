
library(dplyr)
library(ggplot2)
filenames_high <- list.files("../../Test/Landsat_MODIS_Fusion/Landsat_2015/",pattern = ".tif")
filenames_low <- list.files("../../Test/Landsat_MODIS_Fusion/MODIS_2015/",pattern = ".tif")
#drop a couple images for testing purposes
filenames_low <- filenames_low[-c(84,123,158)]

dates_high <- filenames_high %>%
  sapply(substr,18,25) %>%
  as.POSIXct(format="%Y%m%d") %>% 
  strftime(format = "%j") %>% 
  as.numeric()
dates_low <- filenames_low %>% 
  sapply(substr,8,10) %>% 
  as.numeric()

dates_pred <- c(50,51,100,107,123,125,126,128,155,158,200,201,202,280,281,282,283,355)
filenames_pred <- paste("Pred_",dates_pred,".tif",sep = "")



#Make one data frame for the high images and one for the low images
high_df <- data.frame(date = dates_high, has_high = rep(TRUE,length(filenames_high)), files_high = filenames_high,row.names = NULL,stringsAsFactors = F)
low_df <- data.frame(date = dates_low, has_low = rep(TRUE,length(filenames_low)), files_low = filenames_low,row.names = NULL,stringsAsFactors = F)
pred_df <- data.frame(date = dates_pred, predict= rep(TRUE,length(filenames_pred)), files_pred = filenames_pred,row.names = NULL,stringsAsFactors = F)
#join them into different tables #2DO: CHeck later, which of these are actually required
all <- full_join(x = high_df,y = low_df,by=c("date")) %>%
  full_join(y = pred_df,by=c("date")) %>% 
  arrange(date) %>% 
  mutate(date = as.numeric(date)) %>% 
  mutate(has_high = !is.na(has_high)) %>% 
  mutate(has_low = !is.na(has_low)) %>% 
  mutate(predict = !is.na(predict)) %>% 
  mutate(resolutions = factor(paste(has_high,has_low,sep = "-"),levels=c("FALSE-FALSE","FALSE-TRUE","TRUE-TRUE","TRUE-FALSE"))) 
#Find special cases
pairs <- inner_join(x = high_df,y = low_df, by = "date")%>% arrange(date)
low_only <- anti_join(x = low_df,y = high_df, by = "date")%>% arrange(date)
high_only <- anti_join(x = high_df,y = low_df, by = "date")%>% arrange(date)
low_outliers <- low_only[low_only$date>max(pairs$date) | low_only$date<min(pairs$date),]%>% arrange(date)

#Add a column for the six different cases
all<- all %>% mutate(pred_case = case_when(
  predict == FALSE ~ 0,                                      #No Prediction Requested -> No Prediction
  predict == TRUE & has_low == TRUE & has_high==FALSE & !date %in% low_outliers$date ~ 1,   #Only low available -> Normal Prediction
  predict == TRUE & has_low == TRUE & has_high==FALSE & date %in% low_outliers$date ~ 2,                         #Only low available -> Outlier -> Prediction depends on handling.
  predict == TRUE & has_low == FALSE & has_high==FALSE ~ 3,  #Neither High nor low available -> Fail
  predict == TRUE & has_low == TRUE & has_high==TRUE ~ 4,    #Both High and Low available. -> Prediction Unnecessary
  predict == TRUE & has_low == FALSE & has_high==TRUE ~ 5    #Only High available -> Prediction impossible, but unnecessary
)) %>% 
  mutate(interval_breaks = cut(date,breaks = c(-Inf,pairs$date,Inf),right = T,include.lowest = T)) %>% 
  mutate(interval_ids = cut(date,breaks = c(-Inf,pairs$date,Inf),labels = F,right = T,include.lowest = T)) %>% 
  group_by(interval_ids) %>%
  mutate(interval_start = min(date)-1) %>% 
  mutate(interval_end = max(date)) %>% 
  mutate(interval_npred = sum(predict[pred_case==1],na.rm = T)) %>% 
  ungroup()

case_1 <- all$date[all$pred_case==1]
if(length(case_1)){cat(paste("Performing prediction for dates:", case_1))}
case_2 <- all$date[all$pred_case==2]
if(length(case_2)){cat(paste("Detected Outlier dates:", case_2," \nAttempting to work in singlepair mode (currently not implemented)"))}
case_3 <- all$date[all$pred_case==3]
if(length(case_3)){cat(paste("Cannot attempt prediction for dates:", case_3,"\nNo input images were found for those dates."))}
case_4 <- all$date[all$pred_case==4]
if(length(case_4)){cat(paste("Will not attempt prediction for dates", case_4,"\nHigh and low resolution input images were found for those dates, making prediction possible, but unnecessary."))}
case_5 <- all$date[all$pred_case==5]
if(length(case_5)){cat(paste("Will not attempt prediction for dates", case_5,"\nOnly a high resolution input image was found for those dates, making prediction impossible but unnecessary."))}
case_6 <- all$date[all$pred_case==6]


####Find jobs####
valid_job_table <- all %>% 
  group_by(interval_ids,interval_breaks,interval_start,interval_end,interval_npred) %>% 
  filter(sum(interval_npred)>0) %>% 
  summarise()


####Plot Overview####
ggplot(all, aes(x=date, y=resolutions,colour=resolutions,shape=predict,size=predict)) + 
  ggtitle("Task Overview")+
  geom_jitter(alpha=1,width=0,height=0.1) +
  scale_y_discrete(name="",limits=c("FALSE-FALSE","FALSE-TRUE","TRUE-TRUE","TRUE-FALSE"),labels=c("None","Low","Both \n(Pair)","High"))+
  scale_color_discrete(name="Available \nResolutions",labels=c("None","Low","Both \n(Pair)","High"))+
  scale_shape_manual(name="Predict \nDate?",values=c(1,15))+
  scale_size_manual(values=c(2,3),guide="none")+
  geom_rect(data=valid_job_table, aes(xmin=interval_start, xmax=interval_end, ymin=-Inf, ymax=+Inf),
            color=rainbow(length(valid_job_table$interval_end)),
            alpha=0.5,
            inherit.aes = FALSE)


####Predictions####
#For every job
for(i in 1:nrow(valid_job_table)){ 
  #Get the pair dates
  current_job <- valid_job_table[i,]
  start <- current_job$interval_start
  end <-  current_job$interval_end
  current_case_1 <- all[all$interval_ids==current_job$interval_ids & all$pred_case == 1,]
  current_case_2 <- all[all$interval_ids==current_job$interval_ids & all$pred_case == 2,]
  current_case_3 <- all[all$interval_ids==current_job$interval_ids & all$pred_case == 3,]
  current_case_4 <- all[all$interval_ids==current_job$interval_ids & all$pred_case == 4,]
  current_case_5 <- all[all$interval_ids==current_job$interval_ids & all$pred_case == 5,]
  #Start normal doublepair prediction (case1)
  #2DO: Handle other cases
  cat(paste("Starting Job ",i,"predicting in doublepair mode for dates", paste(current_case_1$date,collapse = " ")," in interval ", valid_job_table$interval_breaks[i]))

  
  
}