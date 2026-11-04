
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
SYSTEMC_HOME ?= $(HOME)/systemc

INCLUDES = -I$(SYSTEMC_HOME)/include
LDFLAGS = -L$(SYSTEMC_HOME)/lib-linux64 \
          -Wl,-rpath,$(SYSTEMC_HOME)/lib-linux64
LDLIBS = -lsystemc

# Flash Interface test
test: hi_tb.cpp host_interface.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
	 hi_tb.cpp host_interface.cpp \
	-o test $(LDFLAGS) $(LDLIBS)

# Run the Flash Interface test
run: test
	./test

# Remove generated files
clean:
	rm -f test *.o
