library(plyr)
library(stringr)
library(utils)
library(ggplot2)

read.csv("../data//node18x2a-9-25-15_RANDOM_ALL.csv")->data
data$rideable<-as.factor(gsub("\t","",data$rideable))
ddply(.data=data,.(rideable,threads),mutate,ops_max= max(ops)/(interval*1000000))->data

color_key = c("#362319","#FB5CD6","#23C7A1", "#F23E2B","#7B980D",
        "#1799D0","#6A4B9D","#CB8607","#B90443","#BDBE75",
        "#C0AEE3","#119B4B","#600321","#25314A")
color_key = c("#C144A1","#73D54C", "#87CECF","#D24D32", "#474425",
               "#4B3253","#746DD4","#C38376","#C3C88C","#C58B37",
               "#CFD248","#688CC2","#5B8438","#742C2A","#6CD49B",
               "#507271","#D2B3CA","#CD4976","#CE56DC", "#9A64A5")
names(color_key) <- unique(c(as.character(data$rideable)))
shape_key = c(1:24)
names(shape_key) <- unique(c(as.character(data$rideable)))

chart<-ggplot(data=data,
                  aes(x=threads,y=ops_max,color=rideable, shape=rideable))+
  geom_line()+xlab("Threads")+ylab("Throughput (M ops/sec)")+geom_point(size=2.5)+
  scale_shape_manual(values=shape_key[names(shape_key) %in% data$rideable])+
  theme_bw()+ guides(shape=guide_legend(title=NULL))+ guides(color=guide_legend(title=NULL))+
  scale_color_manual(values=color_key[names(color_key) %in% data$rideable])
chart
