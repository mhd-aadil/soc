
SYSTEMC_HOME ?= /usr/local/systemc-3.0.2

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra \
           -I$(SYSTEMC_HOME)/include \
           -I../ip_01_ram

LDFLAGS = -L$(SYSTEMC_HOME)/lib-linux64 \
          -Wl,-rpath,$(SYSTEMC_HOME)/lib-linux64

LDLIBS = -lsystemc

TARGET = dma_sim

SRCS = dma.cpp dma_tb.cpp ../ip_01_ram/ram.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all run clean
