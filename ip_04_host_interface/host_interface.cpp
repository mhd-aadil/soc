#include "host_interface.h"

void HostInterface::bus_access(uint32_t addr, uint32_t &data, bool is_write)
{
    switch (addr)
    {
    case HI_CMD:

    if (is_write)
    {
        //m_cmd_reg = data & 0xF;

        bool start = (data & (1U << 8)) != 0;

        if (start && (m_status_reg & STATUS_BUSY))
        {
            m_status_reg |= STATUS_ERROR | (ERR_BUSY << 4);
            return;
        }

        m_cmd_reg = data & 0xF;

        if (start)
        {
          
            // Clear previous DONE, ERROR and error code
            m_status_reg &= ~(STATUS_DONE | STATUS_ERROR | (0xFU << 4));

            // Validate request
            uint32_t error_code = validate_request();

            if(error_code != ERR_NONE)
            {
                m_status_reg |=STATUS_ERROR | (error_code<<4);
                return;
            }
            
            m_active_cmd        = m_cmd_reg;
            m_active_flash_addr = m_flash_addr_reg;
            m_active_ram_addr   = m_ram_addr_reg;
            m_active_len        = m_len_reg;

            //Set BUSY
            m_status_reg |= STATUS_BUSY;

            //Wake up orchestrate_thread()
            m_start_req.notify(sc_core::SC_ZERO_TIME);   
           
        }

        
    }
    else
    {
        data = m_cmd_reg & 0xF;
    }

    break;

    case HI_FLASH_ADDR:
        if (is_write)
        {
            m_flash_addr_reg=data;
        }
        else
        {
            data=m_flash_addr_reg;
        }   
        break;

    case HI_RAM_ADDR:
        if (is_write)
        {
            m_ram_addr_reg=data;
        }
            
        else
        {
            
            data=m_ram_addr_reg;
        }
        
        break;

    case HI_LEN:
        if (is_write)
        {
            m_len_reg=data;
        }
        else
        {
            data=m_len_reg;
        }
        break;

    case HI_STATUS:
        if(is_write)
        {
            m_status_reg&= ~(data & 0x6);
        }
        
        else
        {
            data= m_status_reg & 0xff;
        }
        break;

    case HI_IRQ_ENABLE:
        if(is_write)
        {
            m_irq_enable_reg= data & 0x3;
        }
        else
        {
            data = m_irq_enable_reg & 0x3;
        }
        break;
    default:
        if (!is_write)
            data=0;
        break;
    }
}

uint32_t HostInterface::validate_request()
{
    // 1. Validate command
    if (m_cmd_reg > CMD_ERASE)
    {
        return ERR_BAD_CMD;
    }

    // 2. Zero length is invalid for every command
    if (m_len_reg == 0)
    {
        return ERR_BAD_LEN;
    }

    // 3. WRITE/READ length cannot exceed RAM capacity
    if ((m_cmd_reg == CMD_WRITE || m_cmd_reg == CMD_READ) &&
        m_len_reg > RAM_SIZE)
    {
        return ERR_BAD_LEN;
    }

    // 4. Check RAM address range for WRITE/READ
    if ((m_cmd_reg == CMD_WRITE || m_cmd_reg == CMD_READ) &&
        (uint64_t(m_ram_addr_reg) + m_len_reg > RAM_SIZE))
    {
        return ERR_BAD_LEN;
    }

    // 5. Check Flash address range
    if (uint64_t(m_flash_addr_reg) + m_len_reg > FLASH_SIZE)
    {
        return ERR_BAD_ADDR;
    }

    // 6. Check ERASE alignment
    if (m_cmd_reg == CMD_ERASE &&
        (m_len_reg % FLASH_SECTOR_SIZE != 0 ||
         m_flash_addr_reg % FLASH_SECTOR_SIZE != 0))
    {
        return ERR_UNALIGNED;
    }

    return ERR_NONE;
}

HostInterface::HostInterface(sc_core::sc_module_name name, Bus &bus):
    sc_core::sc_module(name),
    hi_done_out("hi_done_out"),
    hi_error_out("hi_error_out"),
    m_bus(bus)
    {
        bus.register_slave("HostInterface",HI_BASE_ADDR,HI_SIZE,this);
        
        hi_done_out.initialize(false);
        hi_error_out.initialize(false);

        SC_THREAD(orchestrate_thread);

    }

    void HostInterface::orchestrate_thread()
    {
        while (true)
        {
            wait(m_start_req);

                if (m_active_cmd == CMD_NOP)
                {
                    // Clear previous completion status
                    m_status_reg &= ~(STATUS_DONE | STATUS_ERROR | (0xFU << 4));

                    // Mark NOP as completed
                    m_status_reg |= STATUS_DONE;

                    // Generate DONE interrupt if enabled
                    if (m_irq_enable_reg & (1U << 0))
                    {
                        hi_done_out.write(true);
                        wait(sc_core::SC_ZERO_TIME);
                        hi_done_out.write(false);
                    }

                    //clear busy
                    m_status_reg &= ~STATUS_BUSY;

                    continue;//skip the remaing and move the top of loop
                }

            // Required for WRITE, READ and ERASE
            m_bus.write(CP_BASE_ADDR + CP_FLASH_ADDR,m_active_flash_addr);

            // Required only for WRITE and READ
            //m_bus.write(CP_BASE_ADDR + CP_RAM_ADDR,m_active_ram_addr);
            if (m_active_cmd == CMD_WRITE || m_active_cmd == CMD_READ)
            {
                m_bus.write(CP_BASE_ADDR + CP_RAM_ADDR,
                            m_active_ram_addr);
            } 
               
            m_bus.write(CP_BASE_ADDR + CP_LEN,m_active_len);

            uint32_t cmd        =(m_active_cmd & 0xFU);
            cmd |= (1U << 8);
            
            m_bus.write(CP_BASE_ADDR + CP_CTRL,cmd);

            uint32_t cp_status = 0;

            do
            {
                m_bus.read(CP_BASE_ADDR + CP_STATUS, cp_status);

                if ((cp_status & 0x6U) == 0)
                {
                    wait(10, sc_core::SC_NS);
                }

            } while ((cp_status & 0x6U) == 0);
            
            if ((cp_status & (0x1U<<2)) != 0)
            {
                uint32_t cp_error_code = (cp_status >> 4) & 0xfu;

                //set ERROR
                m_status_reg |= STATUS_ERROR;

                //store error code in bits [7:4]
                m_status_reg &= ~(0xFU << 4);
                m_status_reg |= cp_error_code << 4;

                if (m_irq_enable_reg & (1U << 1))
                {
                    hi_error_out.write(true);
                    wait(sc_core::SC_ZERO_TIME);
                    hi_error_out.write(false);
                }
            }
            else if ((cp_status & (0x1U<<1)) != 0)
            {        
                // Set DONE
                m_status_reg |= STATUS_DONE;

                if (m_irq_enable_reg & (1U << 0))
                {
                    hi_done_out.write(true);
                    wait(sc_core::SC_ZERO_TIME);
                    hi_done_out.write(false);
                }              
            }
            
            
            // Clear CP DONE and ERROR (W1C)
            uint32_t clear_mask = (1u << 1) | (1u << 2);

            m_bus.write(CP_BASE_ADDR + CP_STATUS,clear_mask);

            // Clear HI BUSY after completion handling
            m_status_reg &= ~STATUS_BUSY;

        }
    }
