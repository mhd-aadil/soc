#include <systemc>
#include <cstdint>
#include "bus.h"

class DmaEngine : public sc_core::sc_module, public IBusSlave
{
public:
    static constexpr uint32_t DMA_BASE_ADDR   = 0x20000200;
    static constexpr uint32_t DMA_SIZE        = 0x18;
    static constexpr uint32_t DMA_STATUS      = 0x00;
    static constexpr uint32_t DMA_BYTES_MOVED = 0x04;
    static constexpr uint32_t DMA_CTRL        = 0x08;
    static constexpr uint32_t DMA_SRC_ADDR    = 0x0c;
    static constexpr uint32_t DMA_DST_ADDR    = 0x10;
    static constexpr uint32_t DMA_LEN         = 0x14;

    static constexpr uint32_t SRC_INC         = (1U << 0);
    static constexpr uint32_t DST_INC         = (1U << 1);
    static constexpr uint32_t START           = (1U << 8);

    static constexpr uint32_t STATUS_BUSY     = (1U << 0);
    static constexpr uint32_t STATUS_DONE     = (1U << 1);
    static constexpr uint32_t STATUS_ERROR    = (1U << 2);


    //DMA ERROR CODE
    static constexpr uint32_t DMA_ERR_NONE    = 0;
    static constexpr uint32_t DMA_ERR_BAD_LEN = 1;
    static constexpr uint32_t DMA_ERR_BUSY    = 2;
    static constexpr uint32_t DMA_ERR_BUS     = 3;


    SC_HAS_PROCESS(DmaEngine);
    DmaEngine(sc_core::sc_module_name name, Bus &bus);
    void bus_access(uint32_t addr, uint32_t &data, bool is_write) override;

private:

    uint32_t m_dma_status      = 0;
    uint32_t m_dma_bytes_moved = 0;
    uint32_t m_dma_ctrl        = 0;
    uint32_t m_dma_src_addr    = 0;
    uint32_t m_dma_dst_addr    = 0;
    uint32_t m_dma_len         = 0;

    Bus &m_bus;

    sc_core::sc_event m_start_event;

    void move_thread();
    void set_error(uint32_t error_code);


    uint32_t m_active_src_addr = 0;
    uint32_t m_active_dst_addr = 0;
    uint32_t m_active_len      = 0;
    uint32_t m_active_ctrl     = 0;


};
