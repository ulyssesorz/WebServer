CXX=g++
CFLAGS=-std=c++14 -O2 -Wall -g
CXXFLAGS=-std=c++14 -O2 -Wall -g

TARGET:=myserver
OBJS = buffer/*.cpp epoller/*.cpp http/*.cpp server/*.cpp timer/*.cpp log/*.cpp main.cpp
$(TARGET):$(OBJS)
	$(CXX) $(CXXFLAGS)  $(OBJS) -o $(TARGET) -pthread