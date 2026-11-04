#include "irq_ctrl.h"

    Interrupt_Controller::Interrupt_Controller(
        sc_core::sc_module_name name, Bus& bus)
        : sc_core::sc_module(name),
          hi_done_in("hi_done_in"),
          hi_error_in("hi_error_in"),
          fi_done_in("fi_done_in"),
          fi_error_in("fi_error_in"),
          cp_done_in("cp_done_in"),
          cp_error_in("cp_error_in"),
          irq_out("irq_out")//hereirq_out is SystemC port name
     {
        bus.register_slave("irq_ctrl",IRQ_BASE_ADDR,IRQ_SIZE,this);

        irq_out.initialize(false);

        SC_METHOD(capture_interrupts);

        sensitive<<hi_done_in.pos()
                 <<hi_error_in.pos()
                 <<fi_done_in.pos()
                 <<fi_error_in.pos()
                 <<cp_done_in.pos()
                 <<cp_error_in.pos();
                 
         dont_initialize();

         SC_METHOD(drive_irq_out);
        sensitive << irq_update_event;
        dont_initialize();
    }

    void Interrupt_Controller::bus_access(
        uint32_t addr, uint32_t &data, bool is_write){
            switch(addr){
                case 0x00:// IRQCTRL_PENDING: read-only 
                    if(!is_write){
                        data=pending;
                    }
                    break;

                case 0x04:// IRQCTRL_MASK: read/write, six valid bits
                    if(is_write){
                        mask=data&0x3f;
                        update_irq_out();
                    }
                    else{
                        data=mask;
                    }
                    break;
                case 0x08: // IRQCTRL_CLEAR: write-one-to-clear
                    if(is_write){
                        pending&=~(data&0x3f);
                        update_irq_out();
                    }
                    else{
                        data=0;
                    }
                    break;
                case 0x0c:// IRQCTRL_RAISE: write-one-to-set
                    if(is_write){
                        pending|=data&0x3f;
                        update_irq_out();
                    }
                    else{
                        data=0;
                    }
                    break;
                default:
                    if(!is_write){
                        data=0;
                    }
                    break;
            }
        }
    
        void Interrupt_Controller::update_irq_out(){
            irq_update_event.notify(sc_core::SC_ZERO_TIME);
            
        }
    void Interrupt_Controller::drive_irq_out()
        {
            irq_out.write((pending & mask) != 0);
            //!=0 means if after (pending&mask) the contain any 1 
            //if yes then irq_out.write(true)  else false
        }

    void Interrupt_Controller::capture_interrupts()
    {
        if (hi_done_in.event())
            pending |= 0x01;

        if (hi_error_in.event())
            pending |= 0x02;

        if (fi_done_in.event())
            pending |= 0x04;

        if (fi_error_in.event())
            pending |= 0x08;

        if (cp_done_in.event())
           pending |= 0x10;

        if (cp_error_in.event())
            pending |= 0x20;

        update_irq_out();
    }
    void Interrupt_Controller::get_mask(){
        uint32_t mask_value=mask;
        std::cout<<"mask register value is:"<<std::hex<<mask_value<<'\n';
    }
 
    void Interrupt_Controller::get_pending(){
        uint32_t pending_value=pending;
        std::cout<<"pending register value is:"<<std::hex<<pending_value<<'\n';
    }
