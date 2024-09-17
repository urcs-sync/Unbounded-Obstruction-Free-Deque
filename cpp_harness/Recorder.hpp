#ifndef RECORDER_HPP
#define RECORDER_HPP

#ifndef _REENTRANT
#define _REENTRANT		/* basic 3-lines for threads */
#endif


#include <map>
#include <list>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <fstream>
#include <iostream>
#include "HarnessUtils.hpp"

class Recorder{

private:
	int task_num;

public:
	// member vars
	std::map<std::string, std::string> globalFields;
	std::map<std::string, std::string>* localFields;

	std::map<std::string, void*> summaryFunctions;

	// these methods must ensure no other threads are accessing 
	// the Recorder concurrently
	Recorder(int task_num);
	void addGlobalField(std::string field);
	void addThreadField(std::string s, std::string (*summarizeFunction)(std::list<std::string>));

	void reportGlobalInfo(std::string field, double value);
	void reportGlobalInfo(std::string field, int value);
	void reportGlobalInfo(std::string field, unsigned long value);
	void reportGlobalInfo(std::string field, std::string value);

	std::string getColumnHeader();
	std::string getData();
	std::string getCSV();
	void outputToFile(std::string outFile);


	// may be called concurrent with other logLocalEntry calls
	void reportThreadInfo(std::string field, double value, int tid);
	void reportThreadInfo(std::string field, int value, int tid);
	void reportThreadInfo(std::string field, std::string value, int tid);


private:
	void summarize();

	bool verifyFile(std::string file);

	static double computeSum(std::list<std::string> list){
		double sum = 0;
		for(std::string s : list){
			sum+=atof(s.c_str());
		}
		return sum;
	}

	static double computeMean(std::list<std::string> list){
		double sum = computeSum(list);
		return sum/list.size();
	}

	static double computeVar(std::list<std::string> list){
		double mean = computeMean(list);
		double temp = 0;
		for(std::string s : list){
			double a = atof(s.c_str());
			temp += (mean-a)*(mean-a);
		}
		return temp/list.size();
	}

	static double computeStdDev(std::list<std::string> list){
		double var = computeVar(list);
		return sqrt(var);
	}

	static std::string ftoa(double f){
		char buff[20];
		sprintf(buff, "%f", f);
		return std::string(buff);
	}

	static std::string itoa(int i){
		char buff[20];
		sprintf(buff, "%d", i);
		return std::string(buff);
	}

	// summary functions
public:
	static std::string sumDoubles(std::list<std::string> list){
		double ans = computeSum(list);
		return std::string(ftoa(ans));
	}
	static std::string sumInts(std::list<std::string> list){
		int ans = (int)computeSum(list);
		return std::string(itoa(ans));
	}
	static std::string avgInts(std::list<std::string> list){
		int ans = (int)computeMean(list);
		return std::string(itoa(ans));
	}
	static std::string avgDoubles(std::list<std::string> list){
		double ans = (double)computeMean(list);
		return std::string(itoa(ans));
	}
	static std::string varInts(std::list<std::string> list){
		int ans = (int)computeVar(list);
		return std::string(itoa(ans));
	}
	static std::string varDoubles(std::list<std::string> list){
		double ans = (double)computeVar(list);
		return std::string(itoa(ans));
	}
	static std::string stdDevInts(std::list<std::string> list){
		int ans = (int)computeStdDev(list);
		return std::string(itoa(ans));
	}
	static std::string stdDevDoubles(std::list<std::string> list){
		double ans = (double)computeStdDev(list);
		return std::string(itoa(ans));
	}

	static std::string concat(std::list<std::string> list){
		std::string sum = "";
		for(std::string s : list){
			sum+=s+":";
		}
		return sum;
	}

	// from:
	// http://stackoverflow.com/questions/16357999/current-date-and-time-as-string
	static std::string dateTimeString(){
		time_t rawtime;
		struct tm * timeinfo;
		char buffer[80];

		time (&rawtime);
		timeinfo = localtime(&rawtime);

		strftime(buffer,80,"%d-%m-%Y %I:%M:%S",timeinfo);
		std::string str(buffer);
		return str;
	}
	
};

#endif
