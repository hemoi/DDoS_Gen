.SUFFIXES: .cpp .o

CXX = g++

INC = 
LIBS = -lpthread -lnet -lpcap

CXXFLAGS = -g $(INC)

OBJS = others.o attacktable.o pktclass.o atkmod.o main.o
SRCS = others.cpp attacktable.cpp pktclass.cpp atkmod.cpp main.cpp

TARGET = ddos

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

dep:
	g++ -MM $(INC) $(SRCS)

clean:
	rm *.o ddos
