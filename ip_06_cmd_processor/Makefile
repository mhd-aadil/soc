
SYSTEMC_HOME ?= /opt/systemc-3.0.2

CXX = g++

CXXFLAGS = -std=c++17 -Wall -Wextra \
           -I$(SYSTEMC_HOME)/include \
           -I../ip_01_ram \
           -I../ip_03_flash_interface \
           -I../ip_05_DMA_ENGINE

LDFLAGS = -L$(SYSTEMC_HOME)/lib-linux64 \
          -Wl,-rpath,$(SYSTEMC_HOME)/lib-linux64

LDLIBS = -lsystemc -pthread

TARGET = cp_sim

SRCS = cmd_processor.cpp cmd_processor_tb.cpp \
       ../ip_01_ram/ram.cpp \
       ../ip_03_flash_interface/flash.cpp \
       ../ip_05_DMA_ENGINE/dma.cpp

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
