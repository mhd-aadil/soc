#ifndef HOST_UART_H
#define HOST_UART_H

#include <systemc>
#include <cstdint>
#include <deque>

#include "bus.h"

class HostUart : public sc_core::sc_module, public IBusSlave
{
public:
    static constexpr uint32_t UART_BASE_ADDR  = 0X20000600;
    static constexpr uint32_t UART_SIZE       = 0X100;
    
    // Register offsets
    static constexpr uint32_t UART_RXDATA     = 0X00;
    static constexpr uint32_t UART_TXDATA     = 0X04;
    static constexpr uint32_t UART_STATUS     = 0X08;
    static constexpr uint32_t UART_CTRL       = 0X0C;
    static constexpr uint32_t UART_CHAR_NS    = 0X10;
    static constexpr uint32_t UART_IRQ_ENABLE = 0X14;
    static constexpr uint32_t UART_RX_COUNT   = 0X18;
    static constexpr uint32_t UART_TX_COUNT   = 0X1C;

    // FIFO configuration
    static constexpr unsigned FIFO_DEPTH      = 16;
    static constexpr unsigned DEFAULT_CHAR_NS = 86806; // 10 bits @ 115200 baud

    // UART status bits
    static constexpr uint32_t RX_NE       = (1U << 0);
    static constexpr uint32_t RX_FULL     = (1U << 1);
    static constexpr uint32_t TX_EMPTY    = (1U << 2);
    static constexpr uint32_t TX_FULL     = (1U << 3);
    static constexpr uint32_t RX_OVERRUN  = (1U << 4);
    static constexpr uint32_t TX_OVERFLOW = (1U << 5);
    static constexpr uint32_t RX_EMPTY_RD = (1U << 6);

    // UART control bits
    static constexpr uint32_t ENABLE   = (1U << 0);
    static constexpr uint32_t RX_FLUSH = (1U << 1);
    static constexpr uint32_t TX_FLUSH = (1U << 2);

    // Interrupt enable bits
    static constexpr uint32_t RX_IRQ_EN    = (1U << 0);
    static constexpr uint32_t ERROR_IRQ_EN = (1U << 1);

    // Serial interfaces
    sc_core::sc_port<sc_core::sc_fifo_in_if<unsigned char>> serial_rx;
    sc_core::sc_port<sc_core::sc_fifo_out_if<unsigned char>> serial_tx;

    // Interrupt outputs
    sc_core::sc_out<bool> uart_rx_irq_out;
    sc_core::sc_out<bool> uart_error_out;

    HostUart(sc_core::sc_module_name name, Bus &bus);

    void bus_access(uint32_t addr, uint32_t &data, bool is_write) override;

 private:
    // SystemC processes
    void rx_thread(); // SC_THREAD — wire -> m_rx
    void tx_thread(); // SC_THREAD — m_tx -> wire

    // Internal FIFOs
    std::deque<unsigned char> m_rx, m_tx;

    uint32_t m_status = 0;
    uint32_t m_ctrl = 1; // ENABLE set out of reset
    uint32_t m_char_ns = DEFAULT_CHAR_NS;
    uint32_t m_irq_en = 0;

    // TX wire activity
    bool m_tx_busy = false;

    // Internal bus reference
    //Bus &bus;

    sc_core::sc_event m_tx_push;

    // Interrupt pulse helper
    void pulse(sc_core::sc_out<bool>& signal, bool enabled);

    sc_core::sc_event m_error_irq_event;
    unsigned int m_error_irq_count = 0;

    void error_irq_thread();
};

#endif