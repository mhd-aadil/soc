
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
SYSTEMC_HOME ?= $(HOME)/systemc

INCLUDES = -I$(SYSTEMC_HOME)/include
LDFLAGS = -L$(SYSTEMC_HOME)/lib-linux64 \
          -Wl,-rpath,$(SYSTEMC_HOME)/lib-linux64
LDLIBS = -lsystemc

# Flash Interface test
test3: flash_tb.cpp flash.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
	flash_tb.cpp flash.cpp \
	-o test3 $(LDFLAGS) $(LDLIBS)

# Run the Flash Interface test
run3: test3
	./test3

# Remove generated files
clean:
	rm -f test3 *.o
