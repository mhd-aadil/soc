#include <systemc>
#include <iostream>
#include <cassert>
#include <cstdint>

#include "bus.h"
#include "ram.h"
#include "flash.h"
#include "dma.h"
#include "cmd_processor.h"

using namespace sc_core;
using namespace std;

SC_MODULE(CommandProcessorTestbench)
{
    Bus& bus;

    SC_HAS_PROCESS(CommandProcessorTestbench);

    CommandProcessorTestbench(sc_module_name name, Bus& bus)
        : sc_module(name), bus(bus)
    {
        SC_THREAD(run_test);
    }

    void run_test()
    {
        wait(SC_ZERO_TIME);

        cout << "\nStarting Command Processor Tests\n";

        // WRITE test will be added here.
        // TEST 1: CP WRITE (RAM -> Flash)

        uint32_t test_data[4] = {
            0x11223344,
            0x55667788,
            0xAABBCCDD,
            0x12345678
        };

        uint32_t ram_addr   = 0x100;
        uint32_t flash_addr = 0xA000;
        uint32_t length     = 16;

        uint32_t cp_status = 0;
        uint32_t fi_status = 0;

        // 1. Load test data into RAM
        for (int i = 0; i < 4; i++)
        {
            assert(bus.write(0x30000000 + ram_addr + (i * 4),
                            test_data[i]));
        }

        // 2. Erase the target flash sector first
        assert(bus.write(0x20000404, flash_addr));
        assert(bus.write(0x20000408, length));
        assert(bus.write(0x20000400, 0x103));

        // Wait for Flash Interface erase completion
        bool erase_done = false;

        for (int i = 0; i < 1000; i++)
        {
            wait(1, SC_NS);

            assert(bus.read(0x2000040C, fi_status));

            if (fi_status & 0x4)
                break;

            if (fi_status & 0x2)
            {
                erase_done = true;
                break;
            }
        }

        assert(erase_done);
        cout << "Flash sector erased successfully\n";

        // 3. Configure Command Processor
        assert(bus.write(0x20000504, flash_addr));
        assert(bus.write(0x20000508, ram_addr));
        assert(bus.write(0x2000050C, length));

        // 4. Start CP WRITE
        assert(bus.write(0x20000500, 0x101));

        // 5. Wait for CP completion
        bool cp_done = false;

        for (int i = 0; i < 1000; i++)
        {
            wait(1, SC_NS);

            assert(bus.read(0x20000510, cp_status));

            if (cp_status & 0x4)
                break;

            if (cp_status & 0x2)
            {
                cp_done = true;
                break;
            }
        }

        assert(cp_done);

        cout << "CP WRITE completed successfully\n";

        // 6. Read Flash data back for verification
        assert(bus.write(0x20000404, flash_addr));
        assert(bus.write(0x20000408, length));
        assert(bus.write(0x20000400, 0x102));

        wait(SC_ZERO_TIME);

        for (int i = 0; i < 4; i++)
        {
            uint32_t read_data = 0;

            assert(bus.read(0x20000410, read_data));

            cout << "Expected: 0x" << hex << test_data[i]
                << " | Actual: 0x" << read_data << dec << endl;

            assert(read_data == test_data[i]);
        }

        // 7. Wait for Flash READ completion
        bool read_done = false;

        for (int i = 0; i < 1000; i++)
        {
            wait(1, SC_NS);

            assert(bus.read(0x2000040C, fi_status));

            if (fi_status & 0x4)
                break;

            if (fi_status & 0x2)
            {
                read_done = true;
                break;
            }
        }

        assert(read_done);

        cout << "\nTEST 1 PASS: CP WRITE RAM -> Flash\n";

        // TEST 2: DMA invalid-length rejection

        cout << "\nStarting TEST 2: DMA Invalid Length\n";

        uint32_t invalid_length = 5;
        uint32_t dma_status = 0;
        uint32_t dma_bytes_moved = 0;

        // 1. Configure DMA source and destination
        assert(bus.write(0x2000020C, 0x30000100));
        assert(bus.write(0x20000210, 0x30000200));

        // 2. Configure invalid length (5 bytes)
        assert(bus.write(0x20000214, invalid_length));

        // 3. Start DMA
        assert(bus.write(0x20000208, 0x103));

        // 4. Read DMA status
        assert(bus.read(0x20000200, dma_status));
        assert(bus.read(0x20000204, dma_bytes_moved));

        cout << "DMA STATUS      = 0x" << hex << dma_status << endl;
        cout << "DMA BYTES MOVED = " << dec << dma_bytes_moved << endl;

        // 5. Verify rejection
        assert((dma_status & 0x1) == 0);  // BUSY = 0
        assert((dma_status & 0x2) != 0);  // DONE = 1
        assert((dma_status & 0x4) != 0);  // ERROR = 1
        assert(((dma_status >> 4) & 0xF) == DmaEngine::DMA_ERR_BAD_LEN);

        assert(dma_bytes_moved == 0);

        cout << "TEST 2 PASS: DMA Invalid Length Rejected\n";

        // TEST 3: DMA Bus Error
        cout << "\nStarting TEST 3: DMA Bus Error\n";

        //uint32_t dma_status = 0;
        //uint32_t dma_bytes_moved = 0;

        // 1. Configure an invalid source address (unmapped address)
        assert(bus.write(0x2000020C, 0x40000000));

        // 2. Configure a valid destination address
        assert(bus.write(0x20000210, 0x30000200));

        // 3. Configure valid length (4 bytes)
        assert(bus.write(0x20000214, 4));

        // 4. Start DMA (SRC_INC=1, START=1)
        assert(bus.write(0x20000208, 0x101));

        // 5. Wait for DMA to finish
        wait(10, SC_NS);

        // 6. Read DMA status and bytes moved
        assert(bus.read(0x20000200, dma_status));
        assert(bus.read(0x20000204, dma_bytes_moved));

        cout << "DMA STATUS      = 0x" << hex << dma_status << endl;
        cout << "DMA BYTES MOVED = " << dec << dma_bytes_moved << endl;

        // 7. Verify bus error
        assert((dma_status & 0x1) == 0);  // BUSY = 0
        assert((dma_status & 0x2) != 0);  // DONE = 1
        assert((dma_status & 0x4) != 0);  // ERROR = 1
        assert(((dma_status >> 4) & 0xF) == DmaEngine::DMA_ERR_BUS);
        assert(dma_bytes_moved == 0);

        cout << "TEST 3 PASS: DMA Bus Error Detected\n";

        // TEST 4: DMA Memory-to-Memory
        {
            cout << "\nStarting TEST 4: DMA Memory-to-Memory\n";

            uint32_t src = 0x30000300;
            uint32_t dst = 0x30000320;
            uint32_t data = 0;

            uint32_t expected[4] = {
                0x11111111, 0x22222222,
                0x33333333, 0x44444444
            };

            // Initialize source memory
            for (int i = 0; i < 4; i++)
                assert(bus.write(src + i * 4, expected[i]));

            // Configure DMA
            assert(bus.write(0x2000020C, src));
            assert(bus.write(0x20000210, dst));
            assert(bus.write(0x20000214, 16));

            // SRC_INC=1, DST_INC=1, START=1
            assert(bus.write(0x20000208, 0x103));

            wait(10, SC_NS);

            // Verify destination memory
            for (int i = 0; i < 4; i++)
            {
                assert(bus.read(dst + i * 4, data));
                cout << "Expected: 0x" << hex << expected[i]
                    << " Actual: 0x" << data << endl;

                assert(data == expected[i]);
            }

            cout << "TEST 4 PASS: DMA Memory-to-Memory\n";
        }

        // TEST 5: DMA Fixed Source
        {
            cout << "\nStarting TEST 5: DMA Fixed Source\n";

            uint32_t src = 0x30000380;
            uint32_t dst = 0x300003A0;
            uint32_t data = 0;

            uint32_t expected = 0xABCDEF12;

            assert(bus.write(src, expected));

            // Configure DMA
            assert(bus.write(0x2000020C, src));
            assert(bus.write(0x20000210, dst));
            assert(bus.write(0x20000214, 12));

            // SRC_INC=0, DST_INC=1, START=1
            assert(bus.write(0x20000208, 0x102));

            wait(10, SC_NS);

            // All three destination words should contain the same value
            for (int i = 0; i < 3; i++)
            {
                assert(bus.read(dst + i * 4, data));
                cout << "Destination[" << dec << i
                    << "] = 0x" << hex << data << endl;

                assert(data == expected);
            }

            cout << "TEST 5 PASS: DMA Fixed Source\n";
        }

        // TEST 6: DMA Fixed Destination
        {
        cout << "\nStarting TEST 6: DMA Fixed Destination\n";

        uint32_t src = 0x300003C0;
        uint32_t dst = 0x300003E0;
        uint32_t data = 0;

        uint32_t expected[3] = {
            0x11112222,
            0x33334444,
            0x55556666
        };

        for (int i = 0; i < 3; i++)
            assert(bus.write(src + i * 4, expected[i]));

        // Configure DMA
        assert(bus.write(0x2000020C, src));
        assert(bus.write(0x20000210, dst));
        assert(bus.write(0x20000214, 12));

        // SRC_INC=1, DST_INC=0, START=1
        assert(bus.write(0x20000208, 0x101));

        wait(10, SC_NS);

        assert(bus.read(dst, data));

        cout << "Expected final word: 0x" << hex << expected[2] << endl;
        cout << "Actual destination: 0x" << data << endl;

        assert(data == expected[2]);

        cout << "TEST 6 PASS: DMA Fixed Destination\n";
        }

        // TEST 7: CP READ Flash to RAM
        {
            cout << "\nStarting TEST 7: CP READ Flash to RAM\n";

            uint32_t flash_addr = 0xA000;
            uint32_t ram_addr = 0x500;
            uint32_t length = 16;
            uint32_t cp_status = 0;
            uint32_t data = 0;

            uint32_t expected[4] = {
                0x11223344,
                0x55667788,
                0xAABBCCDD,
                0x12345678
            };

            // 1. Configure Command Processor
            assert(bus.write(0x20000504, flash_addr));
            assert(bus.write(0x20000508, ram_addr));
            assert(bus.write(0x2000050C, length));

            // 2. Start READ operation
            // OP_READ=2, START=1
            assert(bus.write(0x20000500, 0x102));

            // 3. Poll CP until operation finishes
            for (int i = 0; i < 1000; i++)
            {
                assert(bus.read(0x20000510, cp_status));

                if ((cp_status & 0x1) == 0)
                    break;

                wait(10, SC_NS);
            }

            cout << "CP STATUS = 0x" << hex << cp_status << endl;

            // 4. Verify completion
            assert((cp_status & 0x1) == 0); // BUSY = 0
            assert((cp_status & 0x2) != 0); // DONE = 1
            assert((cp_status & 0x4) == 0); // ERROR = 0

            // 5. Verify RAM contents
            for (int i = 0; i < 4; i++)
            {
                assert(bus.read(0x30000000 + ram_addr + i * 4, data));

                cout << "Expected: 0x" << hex << expected[i]
                    << " Actual: 0x" << data << endl;

                assert(data == expected[i]);
            }

            cout << "TEST 7 PASS: CP READ Flash to RAM\n";
        }


        sc_stop();
    }
};

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Shared Bus
    Bus bus;

    // Instantiate IPs
    Ram ram("ram", bus);

    DmaEngine dma("dma", bus);

    FlashInterface flash_if(
        "flash_if",
        bus,
        "../ip_03_flash_interface/flash.img"
    );

    CommandProcessor cp("cp", bus);

    // Flash Interface signals
    sc_signal<bool> fi_done_sig;
    sc_signal<bool> fi_error_sig;

    flash_if.fi_done_out(fi_done_sig);
    flash_if.fi_error_out(fi_error_sig);

    // Command Processor signals
    sc_signal<bool> cp_done_sig;
    sc_signal<bool> cp_error_sig;

    cp.cp_done_out(cp_done_sig);
    cp.cp_error_out(cp_error_sig);

    // Testbench
    CommandProcessorTestbench tb("tb", bus);

    // Start simulation
    sc_start();

    return 0;
}
