#include "ram.h"

Ram::Ram(sc_core::sc_module_name name, Bus &bus)
 : sc_core::sc_module(name)
{
 bus.register_slave("ram", RAM_BASE, RAM_SIZE, this);
}
void Ram::bus_access(uint32_t addr,
                    uint32_t& data,
                    bool is_write) {
                        if(addr >= RAM_SIZE || RAM_SIZE - addr < 4){
                            if(!is_write)
                            data=0;
                        return;
                        }
                        if(is_write){
                            memory[addr]  =(data>>0) &0xff;
                            memory[addr+1]=(data>>8) &0xff;
                            memory[addr+2]=(data>>16)&0xff;
                            memory[addr+3]=(data>>24)&0xff;
                        }
                        else{
                            data=uint32_t(memory[addr])|
                                 (uint32_t(memory[addr+1])<<8)|
                                 (uint32_t(memory[addr+2])<<16)|
                                 (uint32_t(memory[addr+3])<<24);

                        }
        
                    }
// Temporary test helper; not part of the bus interface.
uint8_t Ram::debug_read_byte(uint32_t addr) const {
    return addr < RAM_SIZE ? memory[addr] : 0;
}        
                



                    