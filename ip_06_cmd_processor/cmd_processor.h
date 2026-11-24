#include <systemc>
#include <cstdint>
#include "bus.h"

class CommandProcessor : public sc_core::sc_module, public IBusSlave
{ 
public:

// Output signals
sc_core::sc_out<bool> cp_done_out;
sc_core::sc_out<bool> cp_error_out;


static constexpr uint32_t CP_BASE_ADDR  = 0x20000500;
static constexpr uint32_t CP_SIZE       = 0x14;

static constexpr uint32_t CP_CTRL       = 0x00;
static constexpr uint32_t CP_FLASH_ADDR = 0x04;
static constexpr uint32_t CP_RAM_ADDR   = 0x08;
static constexpr uint32_t CP_LEN        = 0x0C;
static constexpr uint32_t CP_STATUS     = 0x10;

static constexpr uint32_t OP_WRITE      = 0x1; 
static constexpr uint32_t OP_READ       = 0x2;
static constexpr uint32_t OP_ERASE      = 0x3;

static constexpr uint32_t START         = 0x100; 
static constexpr uint32_t STATUS_BUSY   = 0x1;
static constexpr uint32_t STATUS_DONE   = 0x2;
static constexpr uint32_t STATUS_ERROR  = 0x4;

SC_HAS_PROCESS(CommandProcessor);
CommandProcessor(sc_core::sc_module_name name,Bus &bus);
void bus_access(uint32_t addr, uint32_t &data, bool is_write) override;

private:

    Bus& mbus;

    
    // CP registers
    uint32_t flash_addr = 0;
    uint32_t ram_addr = 0;
    uint32_t length = 0;
    uint32_t ctrl = 0;
    uint32_t status = 0;

    // Active command snapshot
    uint32_t active_op = 0;
    uint32_t active_flash_addr = 0;
    uint32_t active_ram_addr = 0;
    uint32_t active_length = 0;

    // Command execution
    sc_core::sc_event start_event;

    void fsm_thread();

    bool execute_write();
    bool execute_read();
    bool execute_erase();

    bool wait_for_fi();
    bool wait_for_dma();
    void set_error();
    void set_done();

};
