#include "dma.h"

DmaEngine::DmaEngine(sc_core::sc_module_name name,Bus &bus)
    :sc_core::sc_module(name),
     m_bus(bus)
     {
        bus.register_slave("dma",DMA_BASE_ADDR,DMA_SIZE,this);

        SC_THREAD(move_thread);

     }

void DmaEngine::move_thread()
{
    while (true)
    {
        wait(m_start_event);

        uint32_t src_addr = m_active_src_addr;
        uint32_t dst_addr = m_active_dst_addr;

        //bool transfer_error = false;

        for(uint32_t offset = 0; offset < m_active_len; offset += 4)
        {
            uint32_t data = 0;

            if(!m_bus.read(src_addr,data))
            {
                m_dma_status &= ~(0xFU << 4);
                m_dma_status |= STATUS_ERROR | (DMA_ERR_BUS << 4);
                //transfer_error = true;
                break;
            }

            if(!m_bus.write(dst_addr,data))
            {
                m_dma_status &= ~(0xFU << 4);
                m_dma_status |= STATUS_ERROR | (DMA_ERR_BUS << 4);
                //transfer_error = true;
                break;
            }

            m_dma_bytes_moved += 4;

            if(m_active_ctrl & SRC_INC)
                src_addr += 4;

            if(m_active_ctrl & DST_INC)
                dst_addr += 4;
        }
        m_dma_status &= ~STATUS_BUSY;
        m_dma_status |= STATUS_DONE;
    }       
}


void DmaEngine::set_error(uint32_t error_code)
{
    m_dma_status &= ~(0xFU << 4);
    m_dma_status |= STATUS_ERROR | (error_code << 4);
}

void DmaEngine::bus_access(uint32_t addr, uint32_t &data, bool is_write)
{
    switch(addr)
    {
        case DMA_STATUS:
        if(is_write)
        {
            if (data & STATUS_DONE)
                m_dma_status &= ~STATUS_DONE;

            if (data & STATUS_ERROR)
                m_dma_status &= ~(STATUS_ERROR | (0xFU << 4));
                
            //m_dma_status &= ~(data&(STATUS_DONE|STATUS_ERROR));
        }
        else
        {
            data = m_dma_status & 0xf7;
        }
        break;

        case DMA_BYTES_MOVED:
        if(!is_write)
        {
            data = m_dma_bytes_moved;
        }
        break;

        case DMA_CTRL:
        if(is_write)
        {
            bool start = (data & START) != 0;
            m_dma_ctrl = data & 0x3U;

            if(start)
            {
            
                if(m_dma_status & STATUS_BUSY)
                {
                    m_dma_status |= (STATUS_ERROR |(DMA_ERR_BUSY<<4));                    
                    return;
                }

                m_dma_status &= ~(STATUS_ERROR | STATUS_DONE | (0xfU<<4));

                m_dma_bytes_moved = 0;

                if (m_dma_len==0 || m_dma_len % 4 != 0)
                {
                    m_dma_status |= (STATUS_ERROR | STATUS_DONE | (DMA_ERR_BAD_LEN <<4));
                    return; 
                }
            
                m_dma_status |= STATUS_BUSY;

                m_active_src_addr = m_dma_src_addr;
                m_active_dst_addr = m_dma_dst_addr;
                m_active_len      = m_dma_len;
                m_active_ctrl     = m_dma_ctrl;

                m_start_event.notify(sc_core::SC_ZERO_TIME);
            }
                
        }
        
        else
        {
            data = m_dma_ctrl & 0x3;
        }
        break;
        
        case DMA_SRC_ADDR:
        if(is_write)
        {
            m_dma_src_addr = data;
        }
        else
        {
            data = m_dma_src_addr;
        }
        break;

        case DMA_DST_ADDR:
        if(is_write)
        {
            m_dma_dst_addr = data;
        }
        else
        {
            data = m_dma_dst_addr;
        }
        break;

        case DMA_LEN:
        if(is_write)
        {
            m_dma_len = data;
        }
        else
        {
            data = m_dma_len;
        }
        break;

        default:
        if (!is_write)
        {
            data = 0;
        }
    }   
}

