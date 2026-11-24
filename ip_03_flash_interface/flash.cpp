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

    // Check whether the complete flash image was loaded.
    if (file.gcount() != static_cast<std::streamsize>(FLASH_SIZE))
    {
        SC_REPORT_ERROR(
            "FlashInterface",
            "Flash image size is smaller than expected"
        );
        return;
    }

    file.close();

}

bool FlashInterface::save_flash_image()
{
    std::ofstream file(
        image_path,
        std::ios::binary | std::ios::trunc
    );

    if (!file.is_open())
    {
        SC_REPORT_ERROR(
            "FlashInterface",
            "Unable to open flash image for writing"
        );
        return false;
    }

    file.write(
        reinterpret_cast<const char*>(flash.data()),
        static_cast<std::streamsize>(FLASH_SIZE)
    );

    file.flush();

    if (!file)
    {
        SC_REPORT_ERROR(
            "FlashInterface",
            "Failed to write flash image"
        );
        return false;
    }

    return true;
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
        case 0x00: // FI_CTRL
            if (is_write)
            {
                // Ignore CTRL writes while an operation is active.
                if (status & 0x01u)
                {
                    break;
                }

                // Store OP bits [3:0].
                // START bit [8] is not stored.
                ctrl = data & 0x0Fu;

                if (data & (1u << 8))
                {
                    position = 0;
                    transfer_complete = false;

                    // Clear previous DONE and ERROR flags.
                    status &= ~0x6u;

                    operation_valid =
                        (flash_addr <= FLASH_SIZE) &&
                        (length <= FLASH_SIZE - flash_addr);

                    // Set BUSY.
                    status |= 0x1u;

                    start_event.notify(sc_core::SC_ZERO_TIME);
                }
            }
            else
            {
                data = ctrl;
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
        case 0x08: // FI_LEN
            if (is_write)
            {
                length = data;

                // Calculate the number of 32-bit words required.
                uint32_t required_words =
                    (length / 4u) + ((length % 4u) != 0u);

                // Prepare a fresh staging buffer.
                if (length <= FLASH_SIZE)
                {
                    staging_buffer.assign(required_words, 0u);
                    staging_word_index = 0;
                }
                else
                {
                    staging_buffer.clear();
                    staging_word_index = 0;
                }
            }
            else
            {
                data = length;
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

        case 0x10: // FI_DATA
        {
            uint32_t op = ctrl & 0x0Fu;

            // PROGRAM: accept DMA data before START.
            if (op == 0x1u && is_write)
            {
                if (staging_word_index < staging_buffer.size())
                {
                    staging_buffer[staging_word_index] = data;
                    staging_word_index++;
                }
            }

            // READ: return flash data after START.
            else if (op == 0x2u && !is_write)
            {
                if (!operation_valid || position >= length)
                {
                    data = 0;
                    break;
                }

                uint32_t remaining = length - position;
                uint32_t bytes = (remaining < 4u) ? remaining : 4u;

                data = 0;

                for (uint32_t i = 0; i < bytes; i++)
                {
                    uint32_t index = flash_addr + position + i;

                    if (index < FLASH_SIZE)
                    {
                        data |= static_cast<uint32_t>(flash[index])
                                << (i * 8u);
                    }
                }

                position += 4u;

                //uint32_t required_words = (length + 3u) / 4u;
                uint32_t required_words =
                    (length / 4u) + ((length % 4u) != 0u);
                uint32_t transferred_words = position / 4u;

                if (transferred_words >= required_words)
                {
                    transfer_complete = true;
                    transfer_done_event.notify(sc_core::SC_ZERO_TIME);
                }
            }
            else
            {
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
    case 1: // PROGRAM
    {
        // Check whether DMA supplied all required words.
        if (staging_word_index != staging_buffer.size())
        {
            status &= ~0x01u;  // Clear BUSY
            status |= 0x04u;   // Set ERROR

            fi_error_out.write(true);
            wait(sc_core::SC_ZERO_TIME);
            fi_error_out.write(false);
            return;
        }

        // Program the staged data into flash.
        position = 0;

        for (uint32_t i = 0; i < staging_buffer.size(); i++)
        {
            uint32_t word = staging_buffer[i];

            // Process up to four bytes from this word.
            uint32_t remaining = length - position;
            uint32_t bytes = (remaining < 4u) ? remaining : 4u;

            for (uint32_t b = 0; b < bytes; b++)
            {
                uint8_t write_byte =
                    static_cast<uint8_t>((word >> (b * 8u)) & 0xFFu);

                uint32_t index = flash_addr + position + b;

                // Flash programming permits only 1-to-0 transitions.
                flash[index] &= write_byte;
            }

            position += 4u;
        }

        if (!save_flash_image())
        {
            status &= ~0x01u;  // Clear BUSY
            status |= 0x04u;   // Set ERROR

            fi_error_out.write(true);
            wait(sc_core::SC_ZERO_TIME);
            fi_error_out.write(false);
            return;
        }
        break;
    }

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
            flash_addr & ~(FLASH_SECTOR_SIZE - 1u);

        // Calculate the last byte of the requested range.        
        uint32_t last_byte=flash_addr + length - 1u;
        
        // Calculate the last sector containing the requested range.        
        uint32_t last=last_byte & ~(FLASH_SECTOR_SIZE -1u);

        // Erase every sector from first through last, inclusive.
       for (uint32_t s = first; s <= last; s += FLASH_SECTOR_SIZE)
        {
            erase_sector(s);
        }

        if (!save_flash_image())
        {
            status &= ~0x01u;  // Clear BUSY
            status |= 0x04u;   // Set ERROR

            fi_error_out.write(true);
            wait(sc_core::SC_ZERO_TIME);
            fi_error_out.write(false);
            return;
        }

        break;  
    }

    default:
        // Unsupported operation: reject the request.
        status &= ~0x01u;  // Clear BUSY
        status |= 0x04u;   // Set ERROR

        fi_error_out.write(true);
        wait(sc_core::SC_ZERO_TIME);
        fi_error_out.write(false);
        return;
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


