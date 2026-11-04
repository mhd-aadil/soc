#include <systemc>
#include <cassert>
#include <iostream>
#include "host_interface.h"

using namespace sc_core;

SC_MODULE(hostinterface_tb)
{
    Bus& bus;

    sc_in<bool> hi_done_in;
    sc_in<bool> hi_error_in;

    // const uint32_t CMD_REG_ADDR = 0x20000000;
    // const uint32_t FLASH_REG_ADDR = 0x20000004;
    // const uint32_t RAM_REG_ADDR = 0x20000008;
    // const uint32_t LEN_REG_ADDR = 0x2000000C;
    // const uint32_t STATUS_REG_ADDR = 0x20000010;
    // const uint32_t IRQ_EN_REG_ADDR = 0x20000014;

    SC_HAS_PROCESS(hostinterface_tb);

    hostinterface_tb(sc_module_name name,Bus& bus)
       :sc_module(name),
        bus(bus),
        hi_done_in("hi_done_in"),
        hi_error_in("hi_error_in")
    {
        SC_THREAD(run_test);
    }

    void run_test()
    {
        uint32_t read_data = 0;
        uint32_t status = 0;

        // --------------------------------
        // TEST 1: HI_FLASH_ADDR
        // --------------------------------
        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_FLASH_ADDR, 0x1000));

        assert(bus.read(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_FLASH_ADDR, read_data));

        assert(read_data == 0x1000);

        std::cout << "Test 1 passed: HI_FLASH_ADDR\n";


        // --------------------------------
        // TEST 2: HI_RAM_ADDR
        // --------------------------------
        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_RAM_ADDR, 0x100));

        assert(bus.read(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_RAM_ADDR, read_data));

        assert(read_data == 0x100);

        std::cout << "Test 2 passed: HI_RAM_ADDR\n";


        // --------------------------------
        // TEST 3: HI_LEN
        // --------------------------------
        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_LEN, 256));

        assert(bus.read(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_LEN, read_data));

        assert(read_data == 256);

        std::cout << "Test 3 passed: HI_LEN\n";


        // --------------------------------
        // TEST 4: CMD without START
        // --------------------------------
        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_CMD,
                        HostInterface::CMD_READ));

        assert(bus.read(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_CMD, read_data));

        assert(read_data == HostInterface::CMD_READ);

        assert(bus.read(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_STATUS, status));

        assert((status & HostInterface::STATUS_BUSY) == 0);

        std::cout << "Test 4 passed: CMD without START\n";


        // --------------------------------
        // TEST 5: IRQ ENABLE
        // --------------------------------
        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_IRQ_ENABLE, 0x3));

        assert(bus.read(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_IRQ_ENABLE, read_data));

        assert(read_data == 0x3);

        std::cout << "Test 5 passed: IRQ ENABLE\n";


        // --------------------------------
        // TEST 6: Invalid command
        // Expected error code = 4
        // --------------------------------
        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_LEN, 1));

        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_CMD,
                        4U | (1U << 8)));

        assert(bus.read(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_STATUS, status));

        assert((status & HostInterface::STATUS_ERROR) != 0);
        assert(((status >> 4) & 0xFU) == HostInterface::ERR_BAD_CMD);

        std::cout << "Test 6 passed: Invalid command\n";


        // --------------------------------
        // TEST 7: W1C ERROR clearing
        // --------------------------------
        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_STATUS, 0x4));

        assert(bus.read(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_STATUS, status));

        assert((status & HostInterface::STATUS_ERROR) == 0);

        std::cout << "Test 7 passed: W1C ERROR clearing\n";


        // --------------------------------
        // TEST 8: Invalid flash address
        // Expected error code = 1
        // --------------------------------
        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_FLASH_ADDR,
                        HostInterface::FLASH_SIZE - 1));

        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_LEN, 2));

        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_CMD,
                        HostInterface::CMD_READ | (1U << 8)));

        assert(bus.read(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_STATUS, status));

        assert((status & HostInterface::STATUS_ERROR) != 0);
        assert(((status >> 4) & 0xFU) == HostInterface::ERR_BAD_ADDR);

        std::cout << "Test 8 passed: Invalid flash address\n";


        // --------------------------------
        // TEST 9: Invalid RAM length
        // Expected error code = 3
        // --------------------------------
        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_FLASH_ADDR, 0));

        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_RAM_ADDR, 0));

        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_LEN,
                        HostInterface::RAM_SIZE + 1));

        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_CMD,
                        HostInterface::CMD_READ | (1U << 8)));

        assert(bus.read(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_STATUS, status));

        assert((status & HostInterface::STATUS_ERROR) != 0);
        assert(((status >> 4) & 0xFU) == HostInterface::ERR_BAD_LEN);

        std::cout << "Test 9 passed: Invalid RAM length\n";


        // --------------------------------
        // TEST 10: Unaligned ERASE
        // Expected error code = 2
        // --------------------------------
        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_FLASH_ADDR, 1));

        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_LEN,
                        HostInterface::FLASH_SECTOR_SIZE));

        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_CMD,
                        HostInterface::CMD_ERASE | (1U << 8)));

        assert(bus.read(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_STATUS, status));

        assert((status & HostInterface::STATUS_ERROR) != 0);
        assert(((status >> 4) & 0xFU) == HostInterface::ERR_UNALIGNED);

        std::cout << "Test 10 passed: Unaligned ERASE\n";


        // --------------------------------
        // TEST 11: NOP command
        // --------------------------------
        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_FLASH_ADDR, 0));

        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_LEN, 1));

        assert(bus.write(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_CMD,
                        HostInterface::CMD_NOP | (1U << 8)));

        // Allow orchestrate_thread() to process the request
        wait(1, SC_NS);

        assert(bus.read(HostInterface::HI_BASE_ADDR +
                        HostInterface::HI_STATUS, status));

        assert((status & HostInterface::STATUS_BUSY) == 0);
        assert((status & HostInterface::STATUS_DONE) != 0);
        assert((status & HostInterface::STATUS_ERROR) == 0);

        std::cout << "Test 11 passed: NOP command\n";


        std::cout << "\nALL HOST INTERFACE REGISTER AND VALIDATION TESTS PASSED\n";

        sc_stop();
    }
};

int sc_main(int argc,char* argv[])
{
    sc_signal<bool>hi_done_sig;
    sc_signal<bool>hi_error_sig;

    Bus bus;
    HostInterface hi("hi",bus);
    hostinterface_tb hi_tb("hi_tb",bus);

    hi.hi_done_out(hi_done_sig);
    hi_tb.hi_done_in(hi_done_sig);

    hi.hi_error_out(hi_error_sig);
    hi_tb.hi_error_in(hi_error_sig);

    sc_start();

    return 0;

}