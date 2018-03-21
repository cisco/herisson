CXX=g++-5 -std=c++14
LD=g++-5

DEBUG_FLAGS=-g
#DEBUG_FLAGS= -O3 -s -DNDEBUG

VER=$(shell lsb_release -sr)
# convert "xx.xx" to "xx_xx" as we use it as define on code
DOT=.
UNDERSCORE=_
VER2=$(subst $(DOT),$(UNDERSCORE),$(VER))

CFLAGS = -Wno-write-strings -D _UBUNTU_$(VER2) 

LDFLAGS = -L. -lvMI -lpthread -Wl,-rpath,'$$ORIGIN'

%.o: %.cpp
	$(CXX) $(DEBUG_FLAGS) $(CFLAGS) -c -o $@ $<

TARGET = all
all: sample

sample: sample.o
	$(LD) -o $@ $^ $(LDFLAGS) 

clean:
	rm -f *.o $(TARGET)
