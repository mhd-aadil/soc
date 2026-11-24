#include <systemc>
#include <cstdint>
#include "bus.h"

class HostUart : public sc_core::sc_module, public IBusSlave
{
public:
    static constexpr unsigned FIFO_DEPTH = 16;
    static constexpr unsigned DEFAULT_CHAR_NS = 86806; // 10 bits @ 115200 baud

    sc_core::sc_port<sc_core::sc_fifo_in_if<unsigned char>> serial_rx;
    sc_core::sc_port<sc_core::sc_fifo_out_if<unsigned char>> serial_tx;
    sc_core::sc_out<bool> uart_rx_irq_out;
    sc_core::sc_out<bool> uart_error_out;

    HostUart(sc_core::sc_module_name name, Bus &bus);

    void bus_access(uint32_t addr, uint32_t &data, bool is_write) override;

 private:
    void rx_thread(); // SC_THREAD — wire -> m_rx
    void tx_thread(); // SC_THREAD — m_tx -> wire

    std::deque<unsigned char> m_rx, m_tx;

    uint32_t m_status = 0;
    uint32_t m_ctrl = 1; // ENABLE set out of reset
    uint32_t m_char_ns = DEFAULT_CHAR_NS;
    uint32_t m_irq_en = 0;

    sc_core::sc_event m_tx_push;
};