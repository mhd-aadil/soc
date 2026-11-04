#ifndef IRQ_CTRL_H
#define IRQ_CTRL_H
#include <systemc>
#include <cstdint>
#include <iostream>
#include <iomanip>//
#include "bus.h"
class Interrupt_Controller : public sc_core::sc_module, public IBusSlave
{
public:

 SC_HAS_PROCESS(Interrupt_Controller);

 static constexpr uint32_t IRQ_BASE_ADDR = 0x20000300;
 static constexpr uint32_t IRQ_SIZE = 0x10;

 sc_core::sc_in<bool> hi_done_in;
 sc_core::sc_in<bool> hi_error_in;
 sc_core::sc_in<bool> fi_done_in;
 sc_core::sc_in<bool> fi_error_in;
 sc_core::sc_in<bool> cp_done_in;
 sc_core::sc_in<bool> cp_error_in; 

 sc_core::sc_out<bool> irq_out;//here irq_out is C++ variable
 
 Interrupt_Controller(sc_core::sc_module_name name, Bus &bus);

 void bus_access(uint32_t addr, uint32_t &data, bool is_write) override;

 void get_mask();
 
 void get_pending();
 
 
private:
 uint32_t pending=0;
 uint32_t mask=0;

 sc_core::sc_event irq_update_event;


 void update_irq_out();
 void drive_irq_out();
 void capture_interrupts();
};

#endif