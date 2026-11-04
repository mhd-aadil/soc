
#ifndef HOST_INTERFACE_H
#define HOST_INTERFACE_H

#include <systemc>
#include <cstdint>
#include "bus.h"

class HostInterface : public sc_core::sc_module,
                      public IBusSlave
{
public:

    // Base address and register window
    static constexpr uint32_t HI_BASE_ADDR = 0x20000000;
    static constexpr uint32_t HI_SIZE      = 512;

    // Control processor base address and register window
    static constexpr uint32_t CP_BASE_ADDR    = 0x20000500;
    static constexpr uint32_t CP_CTRL         = 0x00;
    static constexpr uint32_t CP_FLASH_ADDR   = 0x04;
    static constexpr uint32_t CP_RAM_ADDR     = 0x08;
    static constexpr uint32_t CP_LEN          = 0x0C;
    static constexpr uint32_t CP_STATUS       = 0x10;


    // Memory limits
    static constexpr uint32_t RAM_SIZE = 4096;
    static constexpr uint32_t FLASH_SIZE = 1024 * 1024;
    static constexpr uint32_t FLASH_SECTOR_SIZE = 4096;

    // Register offsets
    static constexpr uint32_t HI_CMD        = 0x00;
    static constexpr uint32_t HI_FLASH_ADDR = 0x04;
    static constexpr uint32_t HI_RAM_ADDR   = 0x08;
    static constexpr uint32_t HI_LEN        = 0x0C;
    static constexpr uint32_t HI_STATUS     = 0x10;
    static constexpr uint32_t HI_IRQ_ENABLE = 0x14;

    // Commands
    static constexpr uint32_t CMD_NOP   = 0;
    static constexpr uint32_t CMD_WRITE = 1;
    static constexpr uint32_t CMD_READ  = 2;
    static constexpr uint32_t CMD_ERASE = 3;

    // Status bits
    static constexpr uint32_t STATUS_BUSY  = (1U << 0);
    static constexpr uint32_t STATUS_DONE  = (1U << 1);
    static constexpr uint32_t STATUS_ERROR = (1U << 2);

    // Error codes
    enum ErrorCode
    {
        ERR_NONE      = 0,
        ERR_BAD_ADDR  = 1,
        ERR_UNALIGNED = 2,
        ERR_BAD_LEN   = 3,
        ERR_BAD_CMD   = 4,
        ERR_BUSY      = 5
    };

    // Interrupt outputs
    sc_core::sc_out<bool> hi_done_out;
    sc_core::sc_out<bool> hi_error_out;

    SC_HAS_PROCESS(HostInterface);

    HostInterface(sc_core::sc_module_name name, Bus &bus);

    void bus_access(uint32_t addr,
                    uint32_t &data,
                    bool is_write) override;

private:

    // Execution thread
    void orchestrate_thread();

    // Bus reference
    Bus &m_bus;

    // Start event
    sc_core::sc_event m_start_req;

    // Internal registers
    uint32_t m_cmd_reg        = 0;
    uint32_t m_flash_addr_reg = 0;
    uint32_t m_ram_addr_reg   = 0;
    uint32_t m_len_reg        = 0;
    uint32_t m_status_reg     = 0;
    uint32_t m_irq_enable_reg = 0;

    // Validation function
    uint32_t validate_request();

    uint32_t m_active_cmd        = 0;
    uint32_t m_active_flash_addr = 0;
    uint32_t m_active_ram_addr   = 0;
    uint32_t m_active_len        = 0;
};

#endif
