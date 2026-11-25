#include "uart.h"

void HostUart::bus_access(uint32_t addr, uint32_t &data, bool is_write)
{
    switch(addr)
    {
        case UART_RXDATA:
        {
            if(!is_write)
            {
                if(m_rx.size()>0)
                {
                    data=m_rx.front();
                    m_rx.pop_front();
                }
            
                else
                {
                    data = 0;
                    m_status |= RX_EMPTY_RD;
                }
            }
        break;
        }
        case UART_TXDATA:
        {
            if(is_write)
            {
                if (m_tx.size() < FIFO_DEPTH)
                {
                    m_tx.push_back(static_cast<unsigned char>(data));
                    m_tx_push.notify(sc_core::SC_ZERO_TIME);
                }
                else
                {
                    m_status |= TX_OVERFLOW;

                    if (m_irq_en & ERROR_IRQ_EN)
                    {
                        m_error_irq_count++;
                        m_error_irq_event.notify(sc_core::SC_ZERO_TIME);
                    }
                }      
            }
            else
            {
                data = 0;
            }
            break;
        }
        case UART_STATUS:
        if(is_write)
            m_status &= ~(data & 0x70);
        else
        {
            data =m_status & (RX_OVERRUN | TX_OVERFLOW | RX_EMPTY_RD);

            if (!m_rx.empty())
                data |= RX_NE;

            if (m_rx.size() >= FIFO_DEPTH)
                data |= RX_FULL;

            if (m_tx.empty() && !m_tx_busy)
                data |= TX_EMPTY;

            if (m_tx.size() >= FIFO_DEPTH)
                data |= TX_FULL;
        }
        break;

        case UART_CTRL:
        {
            if (is_write)
            { 
                m_ctrl = data & 0x1;

                if(data & RX_FLUSH)
                    m_rx.clear();
                
                if (data & TX_FLUSH)
                {
                    m_tx.clear();
                }
                

            }
                else
                data = m_ctrl & 0x1;
            
            break;
        }
        case UART_CHAR_NS:
        if (is_write)
            m_char_ns =data;
        else 
            data = m_char_ns;
        
        break;

        case UART_IRQ_ENABLE:
        if(is_write)
            m_irq_en = data & 0x3; 
        else
            data = m_irq_en & 0x3;
        break;

        case UART_RX_COUNT:
            if(!is_write)
                data = static_cast<uint32_t>(m_rx.size());
        break;

        case UART_TX_COUNT:
            if(!is_write)
                data = static_cast<uint32_t>(m_tx.size());
        break;

        default:
        if(!is_write)
            data=0;
    }
}

HostUart::HostUart(sc_core::sc_module_name name,Bus &bus):
    sc_core::sc_module(name),
    //bus(bus),
    serial_rx("serial_rx"),
    serial_tx("serial_tx"),
    uart_rx_irq_out("uart_rx_irq_out"),
    uart_error_out("uart_error_out")
    {
        bus.register_slave("HostUart",UART_BASE_ADDR,UART_SIZE,this);
        uart_rx_irq_out.initialize(false);
        uart_error_out.initialize(false);

        SC_THREAD(rx_thread);
        SC_THREAD(tx_thread);
        SC_THREAD(error_irq_thread);
    }

void HostUart::rx_thread()
{
    while (true)
    {
        // serial_rx is connected to an sc_fifo<unsigned char> channel.
        // -> accesses the interface through the port.
        // read() retrieves one byte from the FIFO.
        // If the FIFO is empty, read() blocks until data becomes available.

        unsigned char b = serial_rx->read(); // blocks until the host sends
        
        wait(sc_core::sc_time(m_char_ns, sc_core::SC_NS)); // the character is 'on the wire'
        
        if (!(m_ctrl & 0x1))
            continue; // disabled: byte is discarded
        
        if (m_rx.size() >= FIFO_DEPTH)// NO ROOM — the byte is LOST
        { 
            m_status |= RX_OVERRUN; // RX_OVERRUN, sticky
            // 
            if (m_irq_en & ERROR_IRQ_EN)
            {
                m_error_irq_count++;
                m_error_irq_event.notify(sc_core::SC_ZERO_TIME);
            }
            continue; // note: NOT pushed
        }
        
        m_rx.push_back(b);
        pulse(uart_rx_irq_out, m_irq_en & 0x1);
    }
}

void HostUart::pulse(sc_core::sc_out<bool>& signal, bool enabled)
{
    if (enabled)
    {
        signal.write(true);
        wait(sc_core::SC_ZERO_TIME);
        signal.write(false);
    }
}

void HostUart::tx_thread()
{
    while (true)
    {
        while (m_tx.empty())
            wait(m_tx_push); // Sleep until data is pushed into TX FIFO

        unsigned char b = m_tx.front();
        m_tx.pop_front();

        if (!(m_ctrl & ENABLE))
            continue;

        m_tx_busy = true;

        wait(sc_core::sc_time(m_char_ns, sc_core::SC_NS)); // Character transmission time

        if (m_ctrl & ENABLE)
            serial_tx->write(b);

        m_tx_busy = false;
    }
}

void HostUart::error_irq_thread()
{
    while (true)
    {
        while (m_error_irq_count == 0)
            wait(m_error_irq_event);

        while (m_error_irq_count > 0)
        {
            m_error_irq_count--;
            pulse(uart_error_out, true);
        }
    }
}


