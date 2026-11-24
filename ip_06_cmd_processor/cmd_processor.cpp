#include "cmd_processor.h"

CommandProcessor::CommandProcessor(sc_core::sc_module_name name,Bus &bus)
    :sc_core::sc_module(name),
    cp_done_out("cp_done_out"),
    cp_error_out("cp_error_out"),
     mbus(bus)
     {
        bus.register_slave("CommandProcessor",CP_BASE_ADDR,CP_SIZE,this);

        cp_done_out.initialize(false);
        cp_error_out.initialize(false);

        SC_THREAD(fsm_thread);
     }

void CommandProcessor::bus_access(uint32_t addr,
                                  uint32_t &data,
                                  bool is_write)
{
    switch (addr)
    {
    case CP_CTRL:
        if (is_write)
        {
            ctrl = data & 0xF;

            bool start = (data & START) != 0;

            if (start)
            {
                if (status & STATUS_BUSY)
                {
                    // Already executing; don't overwrite the active command.
                    break;
                }

                status &= ~(STATUS_DONE | STATUS_ERROR);
                status |= STATUS_BUSY;

                active_op         = ctrl & 0xF;
                active_flash_addr = flash_addr;
                active_ram_addr   = ram_addr;
                active_length     = length;

                start_event.notify(sc_core::SC_ZERO_TIME);
            }
        }
        else
        {
            data = ctrl;
        }
        break;

    case CP_FLASH_ADDR:
        if (is_write)
            flash_addr = data;
        else
            data = flash_addr;
        break;

    case CP_RAM_ADDR:
        if (is_write)
            ram_addr = data;
        else
            data = ram_addr;
        break;

    case CP_LEN:
        if (is_write)
            length = data;
        else
            data = length;
        break;

    case CP_STATUS:
        if (is_write)
        {
            status &= ~(data & (STATUS_DONE | STATUS_ERROR));
        }
        else
        {
            data = status;
        }
        break;

    default:
        if (!is_write)
            data = 0;
        break;
    }
}

void CommandProcessor::fsm_thread()
{
   while(true)
   {
   wait(start_event);

      switch (active_op)
      {
      case OP_WRITE:
         if (execute_write())
            set_done();
         else
            set_error();
         break;

      case  OP_READ:
         if (execute_read())
            set_done();
         else
            set_error();
         break;

      case  OP_ERASE:
         if(execute_erase())
            set_done();
         else
            set_error();
         break;
      
      default:
         set_error();
         break;
      }
   }
}


void CommandProcessor::set_error()
{
   status &= ~STATUS_BUSY;
   status |= STATUS_ERROR;

   cp_error_out.write(true);
   wait(sc_core::SC_ZERO_TIME);
   cp_error_out.write(false);
}

void CommandProcessor::set_done()
{
   status &= ~STATUS_BUSY;
   status |= STATUS_DONE;

   cp_done_out.write(true);
   wait(sc_core::SC_ZERO_TIME);
   cp_done_out.write(false);
}

bool CommandProcessor::execute_erase()
{
   if (!mbus.write(0x20000404, active_flash_addr))
      return false;

   if (!mbus.write(0x20000408, active_length))
      return false;

   if (!mbus.write(0x20000400, 0x103))
      return false;

   return wait_for_fi();
}

bool CommandProcessor::wait_for_fi()
{
    uint32_t fi_status = 0;

   while (true)
   {
      if(!mbus.read(0x2000040C, fi_status))
      {
         return false;
      }

      if(fi_status & 0x4)
      {
         return false;
      }

      if(fi_status & 0x2)
      {
         return true;
      }

      wait(1,sc_core::SC_NS);
   }
}

bool CommandProcessor::execute_read()
{
   // Configure and start FI READ
   if(!(mbus.write(0x20000404,active_flash_addr)))
   return false;

   if(!(mbus.write(0x20000408,active_length)))
   return false;

   if(!(mbus.write(0x20000400,0x102)))
   return false;

   // Configure DMA: FI_DATA -> RAM
   if(!(mbus.write(0x2000020C,0x20000410)))
   return false;

   if(!(mbus.write(0x20000210,0x30000000 + active_ram_addr)))
   return false;

   if(!(mbus.write(0x20000214,active_length)))
   return false;

   if(!(mbus.write(0x20000208,0x102)))
   return false;

   bool dma_ok=wait_for_dma();
   bool fi_ok =wait_for_fi();

   return (fi_ok && dma_ok);
} 

bool CommandProcessor::wait_for_dma()
{
   uint32_t dma_status = 0;

   while (true)
   {
      if(!(mbus.read(0x20000200,dma_status)))
         return false;

      if((dma_status & 0x4) != 0)
         return false;

      if((dma_status & 0x2) != 0)
         return true;

      // Unexpected idle state
      if ((dma_status & 0x1) == 0)
         return false;

      wait(1,sc_core::SC_NS);
   }
   
}

bool CommandProcessor::execute_write()
{
    // 1. Configure Flash Interface
    if (!mbus.write(0x20000404, active_flash_addr))
        return false;

    if (!mbus.write(0x20000408, active_length))
        return false;

    // Select PROGRAM operation without starting it
    if (!mbus.write(0x20000400, 0x001))
        return false;

    // 2. Configure DMA: RAM -> FI_DATA
    if (!mbus.write(0x2000020C,
                    0x30000000 + active_ram_addr))
        return false;

    if (!mbus.write(0x20000210, 0x20000410))
        return false;

    if (!mbus.write(0x20000214, active_length))
        return false;

    // SRC_INC = 1, DST_INC = 0, START = 1
    if (!mbus.write(0x20000208, 0x101))
        return false;

    // 3. Wait for DMA
    if (!wait_for_dma())
        return false;

    // 4. Start Flash PROGRAM
    if (!mbus.write(0x20000400, 0x101))
        return false;

    // 5. Wait for Flash completion
    if (!wait_for_fi())
        return false;

    return true;
}

// bool CommandProcessor::execute_write()
// {
//     // 1. Configure DMA: RAM -> FI_DATA
//     if (!mbus.write(0x2000020C, 0x30000000 + active_ram_addr))
//         return false;

//     if (!mbus.write(0x20000210, 0x20000410))
//         return false;

//     if (!mbus.write(0x20000214, active_length))
//         return false;

//     // SRC_INC = 1, DST_INC = 0, START = 1
//     if (!mbus.write(0x20000208, 0x101))
//         return false;

//     // 2. Wait for DMA transfer to complete
//     if (!wait_for_dma())
//         return false;

//     // 3. Configure Flash Interface: PROGRAM
//     if (!mbus.write(0x20000404, active_flash_addr))
//         return false;

//     if (!mbus.write(0x20000408, active_length))
//         return false;

//     // OP_PROGRAM = 1, START = 1
//     if (!mbus.write(0x20000400, 0x101))
//         return false;

//     // 4. Wait for Flash Interface completion
//     if (!wait_for_fi())
//         return false;

//     return true;
// }



