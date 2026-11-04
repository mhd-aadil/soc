
CXX = g++
SYSTEMC_HOME ?= $(HOME)/systemc

CXXFLAGS = -std=c++17 -Wall -Wextra \
           -I$(SYSTEMC_HOME)/include

LDFLAGS = -L$(SYSTEMC_HOME)/lib-linux64 \
          -Wl,-rpath,$(SYSTEMC_HOME)/lib-linux64 \
          -lsystemc -pthread

irq_test: irq_ctrl.cpp irq_tb.cpp irq_ctrl.h bus.h
	$(CXX) $(CXXFLAGS) irq_ctrl.cpp irq_tb.cpp \
		-o irq_test $(LDFLAGS)

run_irq: irq_test
	./irq_test

.PHONY: run_irq


