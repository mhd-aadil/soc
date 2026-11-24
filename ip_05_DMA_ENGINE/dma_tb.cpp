
#include "dma.h"
#include "ram.h"

#include <iostream>
#include <iomanip>

using namespace std;
using namespace sc_core;

SC_MODULE(Dma_tb)
{
    Bus &bus;

    SC_HAS_PROCESS(Dma_tb);

    Dma_tb(sc_module_name name, Bus& bus)
        : sc_module(name),
          bus(bus)
    {
        SC_THREAD(run_test);
    }

    
void run_test()
{
    // DMA register addresses
    const uint32_t DMA_STATUS      = 0x20000200;
    const uint32_t DMA_BYTES_MOVED = 0x20000204;
    const uint32_t DMA_CTRL        = 0x20000208;
    const uint32_t DMA_SRC_ADDR    = 0x2000020C;
    const uint32_t DMA_DST_ADDR    = 0x20000210;
    const uint32_t DMA_LEN         = 0x20000214;

    // DMA control bits
    const uint32_t SRC_INC = (1U << 0);
    const uint32_t DST_INC = (1U << 1);
    const uint32_t START   = (1U << 8);

    // DMA status bits
    const uint32_t BUSY  = (1U << 0);
    const uint32_t DONE  = (1U << 1);
    const uint32_t ERROR = (1U << 2);

    uint32_t data = 0;
    uint32_t status = 0;
    uint32_t bytes_moved = 0;

    // Helper: program, start and wait for one DMA transfer.
    auto run_dma = [&](const char* test_name,
                       uint32_t src,
                       uint32_t dst,
                       uint32_t len,
                       uint32_t ctrl)
    {
        cout << "\n========== " << test_name << " ==========\n";

        bus.write(DMA_SRC_ADDR, src);
        bus.write(DMA_DST_ADDR, dst);
        bus.write(DMA_LEN, len);

        // START is combined with the increment control bits.
        bus.write(DMA_CTRL, ctrl | START);

        // Poll until DMA completes.
        while (true)
        {
            bus.read(DMA_STATUS, status);

            if (status & DONE)
                break;

            wait(1, SC_NS);
        }

        bus.read(DMA_BYTES_MOVED, bytes_moved);

        cout << "DMA STATUS      : 0x" << hex << status << '\n';
        cout << "BYTES MOVED     : " << dec << bytes_moved << '\n';

        if (status & ERROR)
            cout << "RESULT          : ERROR\n";
        else
            cout << "RESULT          : SUCCESS\n";
    };

    // --------------------------------------------------
    // TEST 0: RAM write and read sanity check
    // --------------------------------------------------
    cout << "\n========== RAM TEST ==========\n";

    bus.write(0x30000000, 0x12345678);
    bus.read(0x30000000, data);

    cout << "RAM READ DATA   : 0x"
         << hex << data << '\n';

    // --------------------------------------------------
    // TEST 1: Memory-to-memory copy
    // SRC_INC=1, DST_INC=1
    // --------------------------------------------------
    cout << "\n========== TEST 1: MEMORY TO MEMORY ==========\n";

    bus.write(0x30000100, 0x10000000);
    bus.write(0x30000104, 0x10000001);
    bus.write(0x30000108, 0x10000002);
    bus.write(0x3000010C, 0x10000003);

    run_dma("MEMORY TO MEMORY",
            0x30000100,
            0x30000200,
            16,
            SRC_INC | DST_INC);

    for (uint32_t i = 0; i < 4; i++)
    {
        bus.read(0x30000200 + (i * 4), data);
        cout << "DEST[" << dec << i << "] = 0x"
             << hex << data << '\n';
    }

    // --------------------------------------------------
    // TEST 2: Fixed destination
    // SRC_INC=1, DST_INC=0
    // Last source word should remain at destination.
    // --------------------------------------------------
    cout << "\n========== TEST 2: FIXED DESTINATION ==========\n";

    bus.write(0x30000300, 0xAAAA0000);
    bus.write(0x30000304, 0xAAAA0001);
    bus.write(0x30000308, 0xAAAA0002);
    bus.write(0x3000030C, 0xAAAA0003);

    run_dma("FIXED DESTINATION",
            0x30000300,
            0x30000400,
            16,
            SRC_INC);

    bus.read(0x30000400, data);

    cout << "DESTINATION DATA = 0x"
         << hex << data << '\n';

    // Expected: 0xAAAA0003

    // --------------------------------------------------
    // TEST 3: Fixed source
    // SRC_INC=0, DST_INC=1
    // Same source word should be copied four times.
    // --------------------------------------------------
    cout << "\n========== TEST 3: FIXED SOURCE ==========\n";

    bus.write(0x30000500, 0xBBBB1234);

    run_dma("FIXED SOURCE",
            0x30000500,
            0x30000600,
            16,
            DST_INC);

    for (uint32_t i = 0; i < 4; i++)
    {
        bus.read(0x30000600 + (i * 4), data);
        cout << "DEST[" << dec << i << "] = 0x"
             << hex << data << '\n';
    }

    // Expected: all four words = 0xBBBB1234

    // --------------------------------------------------
    // TEST 4: Fixed source and destination
    // SRC_INC=0, DST_INC=0
    // Same source is repeatedly written to same destination.
    // --------------------------------------------------
    cout << "\n========== TEST 4: FIXED SOURCE AND DESTINATION ==========\n";

    bus.write(0x30000700, 0xCCCC5678);

    run_dma("FIXED SOURCE AND DESTINATION",
            0x30000700,
            0x30000800,
            16,
            0);

    bus.read(0x30000800, data);

    cout << "DESTINATION DATA = 0x"
         << hex << data << '\n';

    // Expected: 0xCCCC5678

    // --------------------------------------------------
    // TEST 5: Invalid length
    // DMA_LEN must be nonzero and multiple of 4.
    // --------------------------------------------------
    cout << "\n========== TEST 5: INVALID LENGTH ==========\n";

    run_dma("INVALID LENGTH",
            0x30000100,
            0x30000200,
            6,
            SRC_INC | DST_INC);

    // Expected: ERROR set, DONE set, bytes moved = 0.

    // --------------------------------------------------
    // TEST 6: Bus error
    // Unmapped source address.
    // --------------------------------------------------
    cout << "\n========== TEST 6: BUS ERROR ==========\n";

    run_dma("UNMAPPED SOURCE",
            0x40000000,
            0x30000900,
            4,
            SRC_INC | DST_INC);

    // Expected: ERROR set, DONE set, bytes moved = 0.

    // --------------------------------------------------
    // Finish simulation
    // --------------------------------------------------
    cout << "\n========== ALL TESTS COMPLETED ==========\n";

    wait(1, SC_NS);
    sc_stop();
}


    // void run_test()
    // {
    //     uint32_t data  = 0;
    //     uint32_t data1 = 0;
    //     uint32_t data2 = 0;
    //     uint32_t data3 = 0;

    //     // Write a 32-bit word into RAM
    //     bool write_ok = bus.write(0x30000000, 0x10000000);

    //     wait(SC_ZERO_TIME);

    //     // Read the word back from RAM
    //     bool read_ok = bus.read(0x30000000, data);

    //     cout << "Write status: " << write_ok << endl;
    //     cout << "Read status : " << read_ok << endl;
    //     cout << "Read data   : 0x"
    //          << hex << data << endl;

    //     wait(SC_ZERO_TIME);
    //     write_ok = bus.write(0x30000000, 0x10000000);
    //     write_ok = bus.write(0x30000004, 0x10000001);
    //     write_ok = bus.write(0x30000008, 0x10000002);
    //     write_ok = bus.write(0x3000000C, 0x10000003);

    //     wait(SC_ZERO_TIME);

    //     write_ok = bus.write(0x2000020C,0x30000000);
    //     write_ok = bus.write(0x20000210,0x30000010);
    //     write_ok = bus.write(0x20000214,16);
    //     write_ok = bus.write(0x20000208, 0x00000103);
        
    //     // Wait until DMA completes
    //     uint32_t status = 0;

    //     while(true)
    //     {
    //         bus.read(0x20000200, status);
    //         if((status & 0x2) != 0)
    //             {
    //                 cout<<"DONE"<<'\n';
    //                 break;
    //             }
    //         if(status & (1U<<2))
    //         {
    //             cout << "DMA ERROR. Status = 0x"
    //                  << hex << status << endl;
    //             break;
    //         }
    //         wait(1,SC_NS);
    //     }
    //     read_ok = bus.read(0x20000204, data);
    //     cout<<dec<<data<<'\n';
        
    //     read_ok = bus.read(0x30000010, data);
    //     read_ok = bus.read(0x30000014, data1);
    //     read_ok = bus.read(0x30000018, data2);
    //     read_ok = bus.read(0x3000001c, data3);

    //     cout<<hex<<data<<'\n';
    //     cout<<hex<<data1<<'\n';
    //     cout<<hex<<data2<<'\n';
    //     cout<<hex<<data3<<'\n';


    //     wait(SC_ZERO_TIME);

    //     sc_stop();
    // }
};

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    Bus bus;

    Ram ram("ram", bus);
    DmaEngine dma("dma", bus);

    Dma_tb tb("tb", bus);

    sc_start();

    return 0;
}

// #include "dma.h"
// #include "ram.h"
// using namespace std;
// using namespace sc_core;


// SC_MODULE(Dma_tb)
// {
//     Bus &bus;

//     SC_HAS_PROCESS(dma_tb);

//     Dma_tb(sc_module_name name,Bus& bus)
//     :sc_module(name),
//      bus(bus)
//      {
//         SC_THREAD(run_test);
//      }

//     void run_test()
//     {
//         uint32_t data = 0;
//         bus.write(0x30000000,0x12345678);
//         wait(SC_ZERO_TIME);
//         bus.read (0x30000000,data);
//         cout<<hex<<data<<endl;
//         wait(SC_ZERO_TIME);
//         sc_stop;

//     }
// };

// int sc_main(int argc,char* argv[])
// {
//     (void)argc;
//     (void)argv;
//     Bus bus;

//     Ram ram("ram", bus);
//     DmaEngine dma("dma",bus);

//     Dma_tb tb("tb",bus);

//     sc_start();

//     return 0;

// }