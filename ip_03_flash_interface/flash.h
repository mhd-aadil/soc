#ifndef FLASH_IF_H
#define FLASH_IF_H

#include <systemc>
#include <cstdint>
#include <array>
#include <string>
#include "bus.h"

class FlashInterface : public sc_core::sc_module, public IBusSlave
{

    SC_HAS_PROCESS(FlashInterface);

public:

 sc_core::sc_out<bool> fi_done_out;
 sc_core::sc_out<bool> fi_error_out;


 static constexpr uint32_t FLASH_BASE_ADDR= 0x20000400;
 static constexpr uint32_t REG_SIZE= 0x14;

 static constexpr uint32_t FLASH_SIZE = 1u * 1024 * 1024;
 static constexpr uint32_t FLASH_SECTOR_SIZE = 4u * 1024;
 
 FlashInterface(sc_core::sc_module_name name, Bus &bus,
 std::string image_path);

 void bus_access(uint32_t addr, uint32_t &data, bool is_write) override;

private:
    std::array<uint8_t, FLASH_SIZE> flash;
    std::string image_path;

    uint32_t flash_addr = 0;
    uint32_t length = 0;
    uint32_t position = 0;
    uint32_t ctrl = 0;
    uint32_t status = 0;

    bool operation_valid = false;
    bool transfer_complete = false;


    sc_core::sc_event start_event;
    sc_core::sc_event transfer_done_event;
    
    void execute_operation();
    void operation_thread();
    void load_flash_image();
    void save_flash_image();
    void erase_sector(uint32_t offset);
};

#endif
