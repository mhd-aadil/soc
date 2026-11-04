CXX = g++

SYSTEMC_HOME ?= $(HOME)/systemc

CXXFLAGS = -std=c++17 -Wall -Wextra \
           -I$(SYSTEMC_HOME)/include

LDFLAGS = -L$(SYSTEMC_HOME)/lib-linux64 \
          -Wl,-rpath,$(SYSTEMC_HOME)/lib-linux64 \
          -lsystemc -pthread

COMMON = ram.cpp
HEADERS = ram.h bus.h

.PHONY: all test1 test2 clean

all: test1

test1: $(COMMON) sc_main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(COMMON) sc_main.cpp -o ram_test1 $(LDFLAGS)
	./ram_test1

test2: $(COMMON) ram_tb.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(COMMON) ram_tb.cpp -o ram_test2 $(LDFLAGS)
	./ram_test2

clean:
	rm -f ram_test1 ram_test2


# CXX = g++

# SYSTEMC_HOME ?= $(HOME)/systemc

# CXXFLAGS = -std=c++17 -Wall -Wextra \
#            -I$(SYSTEMC_HOME)/include

# LDFLAGS = -L$(SYSTEMC_HOME)/lib-linux64 \
#           -Wl,-rpath,$(SYSTEMC_HOME)/lib-linux64 \
#           -lsystemc -pthread

# TARGET = ram_test
# SOURCES = ram.cpp ram_tb.cpp

# all: $(TARGET)

# $(TARGET): $(SOURCES) ram.h bus.h
# 	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

# run: $(TARGET)
# 	./$(TARGET)

# clean:
# 	rm -f $(TARGET)

# .PHONY: all run clean
