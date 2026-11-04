#include "flash.h"
#include <fstream>
#include <utility>

FlashInterface::FlashInterface(sc_core::sc_module_name name,
                   Bus& bus,
                   std::string image_path):
                   sc_core::sc_module(name),
                   fi_done_out("fi_done_out"),
                   fi_error_out("fi_error_out"),
                   image_path(std::move(image_path))
                   //std::move() does not itself move anything. 
                   // It enables a move operation if the selected constructor 
                   // or assignment operator supports it.


    {

    bus.register_slave("FlashInterface", FLASH_BASE_ADDR, REG_SIZE, this);

    // Initialize interrupt outputs to defined values.
    fi_done_out.initialize(false);
    fi_error_out.initialize(false);

    // Load an existing image or initialize a fresh flash.
    load_flash_image();

    // We'll register the operation process here next.
    SC_THREAD(operation_thread);
}
    
void FlashInterface::load_flash_image()
{
    // A new flash device starts erased.
    flash.fill(0xff);

    //This creates an input file stream named file and
    //opens the file specified by image_path in binary read mode.
    std::ifstream file(image_path, std::ios::binary);

    if (!file.is_open()) {
        // No existing image: create a fresh erased image.
        save_flash_image();
        return;
    }

    //file.read(destination_pointer, number_of_bytes);
    //flash.data() method returns a pointer to the first element
    //of the array.
    file.read(reinterpret_cast<char*>(flash.data()),
              //The read() function expects a char* destination pointer.
              //But flash.data() returns a uint8_t*
              static_cast<std::streamsize>(FLASH_SIZE)
              //The read() function expects the number of bytes to read
              //as a std::streamsize.
    );

    file.close();

}

void FlashInterface::save_flash_image()
{
    //ofstream means output file stream.
    //It is used to write data to a file.
    //file is the C++ object that manages writing to the file.
    std::ofstream file(
    image_path,//This is the file path supplied to your constructor.
    //Opens the file in binary mode.
    std::ios::binary | std::ios::trunc
    //trunc means truncate. It removes the existing file contents
    //when the file is successfully opened.
    );
    if (!file.is_open()) {
        SC_REPORT_ERROR(
            "FlashInterface",
            "Unable to open flash image for writing"
        );
        return;
    }
    //file.write(source_pointer, number_of_bytes);
    file.write(
        reinterpret_cast<const char*>(flash.data()),
        static_cast<std::streamsize>(FLASH_SIZE)
    );

    //C++ may temporarily hold the data in a memory buffer
    //before sending it to the file.
    //flush() asks it to push that buffered data
    //to the file immediately.
    file.flush();

    //check if the stream has an error state, such as 
    //a failed write or flush.
    if (!file) {
        SC_REPORT_ERROR(
            "FlashInterface",
            "Failed to write flash image"
        );
    }

}

void FlashInterface::bus_access(uint32_t addr,
                                uint32_t& data,
                                bool is_write)
{
    // (void)addr;
    // (void)data;
    // (void)is_write;
    switch(addr)
    {   
        case 0x00:// FI_CTRL
        if(is_write)
        {
            ctrl=data&0xfu;// Store OP bits [3:0]
            
            if(data&(1u<<8)) // START bit
            {   
                position=0;
                transfer_complete = false;
                status&=~0x6u;// Clear DONE and ERROR
                operation_valid =
                    (flash_addr <= FLASH_SIZE) &&
                    (length <= FLASH_SIZE - flash_addr);
                status|=1u;   // Set BUSY

                start_event.notify(sc_core::SC_ZERO_TIME);
            }  
        }
        else
        {
            data=ctrl;
        }
        break;

        case 0x04://FI_FLASH_ADDR
        if (is_write)
        {
            flash_addr=data;
        }
        else
        {
            data=flash_addr;
        }
        break;
        case 0x08://FI_LEN
        if (is_write)
        {
            length=data;
        }
        else
        {
            data=length;
        }
        break;
        case 0x0c:// FI_STATUS
        if(is_write)
        {
            status&= ~(data&0x6u);
        }
        else
        {
            data=status&0x7u;
        }
        break;

        case 0x10:
        {
            uint32_t op=ctrl & 0xfu;

            if (!operation_valid) {
                data = 0;
                break;
            }

            // Number of valid bytes remaining in this operation.
            if(position>=length)
            {
                data=0;
                break;
            }

            uint32_t remaining =length-position;
            uint32_t bytes=(remaining<4u)?remaining:4u;

            if(op==0x1u&&is_write)
            {
                
                for(uint32_t i=0;i<bytes;i++)
                {
                    uint32_t index=flash_addr+position+i;
                    if(index<FLASH_SIZE)
                    {
                        uint8_t write_byte=
                            static_cast<uint8_t>((data>>(i*0x8u))&0xffu);
                            flash[index]&=write_byte;
                    }
                }
                position+=4;
                uint32_t required_words = (length + 3u) / 4u;
                //The formula for rounding up integer division is:
                //[n/d]=(n+d-1)/d
                //for our cased=4
                //words=(length+3)
                uint32_t transferred_words = position / 4u;

                if (transferred_words >= required_words) {
                    transfer_complete = true;
                    transfer_done_event.notify(sc_core::SC_ZERO_TIME);
                }
            }

            else if (op==0x2u&& !is_write)
            {
                //READ: pack flash bytes into a 32-bit word.
                data=0;

                for (uint32_t i = 0; i < bytes; i++)
                {
                    uint32_t index=flash_addr+position+i;
                    if(index<FLASH_SIZE)
                    {
                        data|=static_cast<uint32_t>(flash[index])<<(i*8u);
                    }
                }

                position+=4u;
                uint32_t required_words = (length + 3u) / 4u;
                //The formula for rounding up integer division is:
                //[n/d]=(n+d-1)/d
                //for our cased=4
                //words=(length+3)
                uint32_t transferred_words = position / 4u;

                if (transferred_words >= required_words) {
                    transfer_complete = true;
                    transfer_done_event.notify(sc_core::SC_ZERO_TIME);
                }
            }

            else
            {
               // Unsupported access direction or operation.
                data = 0; 
            }
            break;
        }



        default:
            data=0;
            break;
    }
}

void FlashInterface::operation_thread()
{
    while(true)
    {
        wait(start_event);
        execute_operation();
    }
}

void FlashInterface::execute_operation()
{
    uint32_t op = ctrl & 0x0Fu;
    // Validate the requested flash address range.
    if (flash_addr > FLASH_SIZE ||
        length > FLASH_SIZE - flash_addr) {

        // Invalid range: don't access flash.
        status &= ~0x01u;  // Clear BUSY
        status |= 0x04u;   // Set ERROR

        fi_error_out.write(true);
        wait(sc_core::SC_ZERO_TIME);
        fi_error_out.write(false);
        return;
    }

    switch (op)
    {
    case 1://PROGRAM
        if (length > 0 && !transfer_complete) {
            wait(transfer_done_event);
        }

        save_flash_image();
        break;

    case 2://read
        if(length>0 && !transfer_complete){
            wait(transfer_done_event);
        }
        break;

    case 3://erase
    {
        if(length==0){
            break;
        }

        // Calculate the first sector containing the requested range.
        uint32_t first=
            flash_addr & ~(FLASH_SECTOR_SIZE-0X1u);

        // Calculate the last byte of the requested range.        
        uint32_t last_byte=flash_addr + length - 1u;
        
        // Calculate the last sector containing the requested range.        
        uint32_t last=last_byte & ~(FLASH_SECTOR_SIZE -1u);

        // Erase every sector from first through last, inclusive.
        for (uint32_t s = first; s <= last; s=s+FLASH_SECTOR_SIZE)
        {
            erase_sector(s);   
        }

        save_flash_image();

        break;   
    }

    default:
        //Unsupported operation: do not modify flash.
        break;
    }

    // Mark the operation as finished.
    status &= ~0x01u;  // Clear BUSY
    status |= 0x02u;   // Set DONE

    //Generate a one-delta-cycle completion pulse.
    fi_done_out.write(true);
    wait(sc_core::SC_ZERO_TIME);
    fi_done_out.write(false);      
}

void FlashInterface::erase_sector(uint32_t offset)
{
    uint32_t sector_start = 
        offset & ~(FLASH_SECTOR_SIZE - 1u);
        //FLASH_SECTOR_SIZE - 1u gives 0xFFF.
        //~0xFFF clears the lowest 12 bits

    for (uint32_t i=0; i< FLASH_SECTOR_SIZE;i++)
    {
    flash[sector_start+i] = 0xff;
    }
}


