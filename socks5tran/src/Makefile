cCC = g++

DEBUG=1
STATIC=1

ELF = server
ROOTSRC = $(wildcard *.cpp)
ROOTOBJ = $(ROOTSRC:%.cpp = %.o)

ifeq ($(DEBUG),1)
PARAMETER1 = -g
else
PARAMETER1 = -O2
endif

ifeq ($(STATIC),1)
PARAMETER2 = -static
else
PARAMETER2 = 
endif

$ELF:$(ROOTOBJ)
	$(cCC) $(PARAMETER1) $(PARAMETER2) -o $(ELF) $(ROOTOBJ) -lpthread
clean:
	rm -rf *.o $(ELF)	
