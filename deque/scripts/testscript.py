#!/usr/bin/python
from os.path import dirname, realpath, sep, pardir
import sys
import os

# execution ----------------
#print dirname(realpath(__file__))
os.environ['PATH'] = dirname(realpath(__file__))+":" + os.environ['PATH'] # scripts
os.environ['PATH'] = dirname(realpath(__file__))+"/..:" + os.environ['PATH'] # bin
os.environ['PATH'] = dirname(realpath(__file__))+"/../../cpp_harness:" + os.environ['PATH'] # metacmd
for i in range(0,1):
	#cmd = "metacmd.py dq -i 3 -m 4 --meta d:'access_type=STACK':'access_type=QUEUE':'access_type=RANDOM' -v --meta t:1...4:6:8:16:20:24:32:40:48:64 --meta r:0:1:2:3:5 -o ./data/node18x2a-10-8-15.csv"
	# cmd = "metacmd.py dq -i 3 -m 4 --meta d:'access_type=STACK':'access_type=QUEUE':'access_type=RANDOM' -v --meta t:5:7:9...15:17:18:19:21:22:23 --meta r:0:1:2:3:5 -o ./data/node18x2a-10-8-15.csv"
	#cmd = "metacmd.py dq -i 3 -m 4 --meta d:'access_type=STACK':'access_type=QUEUE':'access_type=RANDOM' -v --meta t:1...24:32:40:48:64 --meta r:0:1:2:3:5:6 -o ./data/cycle3-10-9-15-prefill.csv -d prefill=500"
	cmd = "metacmd.py dq -i 3 -m 4 --meta d:'access_type=STACK':'access_type=QUEUE':'access_type=RANDOM' -v --meta t:1...24:28:32:36:40:44:48:52:56:64 --meta r:3 -o ./data/node18x2a-2-11-2016.csv "
	os.system(cmd)
	#cmd = "metacmd.py dq -i 2 -m 4 -d 'access_type=QUEUE' -v --meta t:1...4:6:8:16:18:20:24:32:36:40:48:64:72:100 --meta r:0:1:2:3:5:6:7:8:9:10 -o ./data/node18x2a-9-25-15_QUEUE_ALL_3.csv"
	#os.system(cmd)
