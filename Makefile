# Makefile Sunny, Alex, Daniel

LIBS = -lpthread
CXX = g++

all: proxy

 proxy: proxy.cpp
	$(CXX) proxy.cpp -o proxy $(LIBS)
