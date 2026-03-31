CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

code: compiler.cpp
	$(CXX) $(CXXFLAGS) -o code compiler.cpp

clean:
	rm -f code

.PHONY: clean
