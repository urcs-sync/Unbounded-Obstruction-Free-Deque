# Makefile built from 
# http://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

IDIR =./
CC=g++

# -DLEVEL1_DCACHE_LINESIZE detects the cache line size and passes it in as a compiler flag

CFLAGS=-I$(IDIR) -I ./include -I ../cpp_harness -I scal-master/src/ -I scal-master/ -m32 -Wno-write-strings -fpermissive -pthread -DLEVEL1_DCACHE_LINESIZE=`getconf LEVEL1_DCACHE_LINESIZE`

# Additional options for different builds:

# gcov build
# -fprofile-arcs -ftest-coverage -O0 
# test build
#-DNDEBUG -O3  
#debug build
#-g -rdynamic 
# line by line debug coverage (access via command line: gprof -l)
#-O0 -pg -g 

CFLAGS+=-O3 -ggdb


ODIR=./obj

LIBS=-lpthread -lharness

_DEPS = RDeque.hpp Tests.hpp OFDeque.hpp WSDeque.hpp MMDeque.hpp FCDeque.hpp SGLDeque.hpp ElimTable.hpp
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ =  Tests.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

_SCAL_OBJ = allocation.o random.o threadlocals.o
SCAL_OBJ = $(patsubst %,$(ODIR)/%,$(_SCAL_OBJ))

$(ODIR)/%.o: %.cpp $(DEPS) 
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

all: dq

dq: $(ODIR)/Main.o  $(OBJ) $(SCAL_OBJ)
	make -C ../cpp_harness
	g++ -o $@ $^ $(CFLAGS) -L ../cpp_harness $(LIBS)

$(ODIR)/allocation.o: scal-master/src/util/allocation.cc $(DEPS) 
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/random.o: scal-master/src/util/random.cc $(DEPS) 
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/threadlocals.o: scal-master/src/util/threadlocals.cc $(DEPS) 
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ dq

