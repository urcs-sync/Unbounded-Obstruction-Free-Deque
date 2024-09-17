#ifndef DEFAULT_HARNESS_TESTS_HPP
#define DEFAULT_HARNESS_TESTS_HPP

#ifndef _REENTRANT
#define _REENTRANT		/* basic 3-lines for threads */
#endif

#include "RAllocator.hpp"
#include "RContainer.hpp"
#include "Harness.hpp"
#include <vector>
#include <list>

class InsertRemoveTest : public Test{
public:
	RContainer* q;
	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc);
};


class NothingTest : public Test{
public:
	Rideable* r;
	void init(GlobalTestConfig* gtc){r = gtc->allocRideable();}
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){return 0;}
	void cleanup(GlobalTestConfig* gtc){}
};



class FAITest :  public Test{
public:
	unsigned long int fai_cntr;
	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc){}
};


class NearEmptyTest :  public Test{
public:
	RContainer* q;
	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc){}
};

class AllocatorChurnTest :  public Test{
public:
	RAllocator* bp;
	std::vector<std::list<void*>*> leftovers;
	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc);
};

class SequentialTest : public Test{
public:

	virtual int execute(GlobalTestConfig* gtc)=0;
	virtual void init(GlobalTestConfig* gtc)=0;
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	virtual void cleanup(GlobalTestConfig* gtc)=0;

};

class SequentialUnitTest : public SequentialTest{
public:

	bool passed=true;


	virtual int execute(GlobalTestConfig* gtc)=0;
	virtual void init(GlobalTestConfig* gtc)=0;
	void cleanup(GlobalTestConfig* gtc);
	virtual void clean(GlobalTestConfig* gtc)=0;
	void verify(bool stmt);

};


class UIDGenerator{
	
	int taskNum;
	int tidBits;
	uint32_t inc;

public:
	UIDGenerator(int taskNum);
	uint32_t initial(int tid);
	uint32_t next(uint32_t prev, int tid);

	uint32_t count(uint32_t val);
	uint32_t id(uint32_t val);


};


#endif
