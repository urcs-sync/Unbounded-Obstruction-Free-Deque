library(plyr)
library(stringr)
library(utils)
library(ggplot2)

# here's where we read in a csv
read.csv("./data/cycle3-10-9-15.csv")->data

# get rid of stupid tabs
data$rideable<-as.factor(gsub("\t","",data$rideable))
data$environment<-as.factor(gsub("\t","",data$environment))

# find the summary statistic

# here's the max
#ddply(.data=data,.(rideable,threads,environment),mutate,ops_max= max(ops)/(interval*1000000))->data
# here's the mean
ddply(.data=data,.(rideable,threads,environment),mutate,ops_max= mean(ops)/(interval*1000000))->data


# these build custom color and shape keys
# if you want to change what series is what color/shape
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

# I tried to give a bunch
# of graph formatting options here.
# a good reference for this is:
# http://docs.ggplot2.org/0.9.2.1/theme.html

# build a chart
chart<-
  
  # ---- ggplot sets the data and what columns are used for which dimension
  # (x,y,color,shape, etc.)
  ggplot(
  # set what data you want, e.g. 
  # take a subset of the data where the environment contains the string "RANDOM"
  # and the number of threads is less than 24
  data=subset(data,str_detect(environment,"RANDOM") & threads<=24),
  # set what column's are plotted as x, y, color, and shape values
                  aes(x=threads,y=ops_max,color=rideable, shape=rideable))+
  
  # ----- the remainder of these options set the chart format
  
  # plot data using lines
  geom_line()+
  # plot data using points, and set the size of the points
  geom_point(size=2.5)+
  # use black and white theme as base
  theme_bw()+ 
  # x axis label
  xlab("Threads")+
  # y axis label
  ylab("Throughput (M ops/sec)")+
  # format axis titles
  theme(axis.title = element_text(size=12, face="bold"))+
  # format axis labels
  theme(axis.text = element_text(size=12, face="bold"))+
  # for custom control of shape
  #scale_shape_manual(values=shape_key[names(shape_key) %in% data$rideable])+
  # for custom control of color
  #scale_color_manual(values=color_key[names(color_key) %in% data$rideable])+
  # don't title the shape legend
  guides(shape=guide_legend(title=NULL))+ 
  # don't title the line legend
  guides(color=guide_legend(title=NULL))+
  # format legend text
  theme(legend.text = element_text(size=12, face="bold"))+
  # set title
  #ggtitle("Graph title")+
  # format title
  #theme(plot.title = element_text(size=18, face="bold"))
  # null to end append
  NULL
  
# show the chart
chart

# save the chart (width, height in inches)
ggsave(plot=chart, file="chart.png", width=8, height=5, dpi=300)
