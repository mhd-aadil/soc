#include <systemc>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <cassert>

#include "bus.h"
#include "flash.h"

using namespace sc_core;
using namespace std;

SC_MODULE(FlashTestbench)
{
    Bus& bus;

    sc_in<bool> fi_done;
    sc_in<bool> fi_error;

    SC_HAS_PROCESS(FlashTestbench);

    static constexpr uint32_t CTRL   = 0x20000400;
    static constexpr uint32_t ADDR   = 0x20000404;
    static constexpr uint32_t LEN    = 0x20000408;
    static constexpr uint32_t STATUS = 0x2000040C;
    static constexpr uint32_t DATA   = 0x20000410;

    static constexpr uint32_t START_BIT = 1u << 8;
    static constexpr uint32_t BUSY_BIT  = 1u << 0;
    static constexpr uint32_t DONE_BIT  = 1u << 1;
    static constexpr uint32_t ERROR_BIT = 1u << 2;

    FlashTestbench(sc_module_name name, Bus& bus)
        : sc_module(name),
          bus(bus),
          fi_done("fi_done"),
          fi_error("fi_error")
    {
        SC_THREAD(run_test);
    }

    // Wait for BUSY to clear. Fail if the operation takes too long.
    uint32_t wait_for_completion()
    {
        uint32_t status = 0;

        for (unsigned int i = 0; i < 100; ++i)
        {
            wait(1, SC_NS);

            assert(bus.read(STATUS, status));

            if ((status & BUSY_BIT) == 0)
                return status;
        }

        assert(false && "Flash operation timed out");
        return status;
    }

    void check_success(const char* test_name)
    {
        uint32_t status = wait_for_completion();

        cout << test_name << " STATUS = 0x"
             << hex << status << dec << endl;

        assert((status & DONE_BIT) != 0);
        assert((status & ERROR_BIT) == 0);
        assert((status & BUSY_BIT) == 0);
    }

    void erase_flash(uint32_t address, uint32_t length)
    {
        assert(bus.write(ADDR, address));
        assert(bus.write(LEN, length));
        assert(bus.write(CTRL, START_BIT | 3u));

        check_success("ERASE");
    }

    void program_flash(uint32_t address,
                       uint32_t length,
                       const uint32_t* words,
                       unsigned int word_count)
    {
        assert(bus.write(ADDR, address));
        assert(bus.write(LEN, length));
        assert(bus.write(CTRL, START_BIT | 1u));

        // Let the operation thread process START.
        wait(SC_ZERO_TIME);

        for (unsigned int i = 0; i < word_count; ++i)
        {
            assert(bus.write(DATA, words[i]));
        }

        check_success("PROGRAM");
    }

    void read_flash(uint32_t address,
                    uint32_t length,
                    uint32_t* words,
                    unsigned int word_count)
    {
        assert(bus.write(ADDR, address));
        assert(bus.write(LEN, length));
        assert(bus.write(CTRL, START_BIT | 2u));

        // Let the operation thread process START.
        wait(SC_ZERO_TIME);

        for (unsigned int i = 0; i < word_count; ++i)
        {
            words[i] = 0;
            assert(bus.read(DATA, words[i]));
        }

        check_success("READ");
    }

    void run_test()
    {
        uint32_t value = 0;
        uint32_t words[2] = {};

        cout << "\nStarting IP-03 Flash Interface tests\n";
        wait(SC_ZERO_TIME);

        // ------------------------------------------------
        // TEST 1: ERASE the first sector.
        // This makes the test independent of old flash.img
        // contents at address zero.
        // ------------------------------------------------
        erase_flash(0, 4);
        read_flash(0, 4, &value, 1);

        assert(value == 0xFFFFFFFFu);
        cout << "TEST 1 PASS: ERASE and erased data\n";

        // ------------------------------------------------
        // TEST 2: PROGRAM and READ four bytes.
        // ------------------------------------------------
        words[0] = 0x12345678u;
        program_flash(0, 4, words, 1);

        read_flash(0, 4, &value, 1);
        assert(value == 0x12345678u);

        cout << "TEST 2 PASS: 4-byte PROGRAM and READ\n";

        // ------------------------------------------------
        // TEST 3: Real flash programming semantics.
        // A second PROGRAM must not restore bits from 0 to 1.
        // ------------------------------------------------
        erase_flash(0, 4);

        words[0] = 0x0F0F0F0Fu;
        program_flash(0, 4, words, 1);

        words[0] = 0xFFFFFFFFu;
        program_flash(0, 4, words, 1);

        read_flash(0, 4, &value, 1);
        assert(value == 0x0F0F0F0Fu);

        cout << "TEST 3 PASS: Flash bitwise-AND programming\n";

        // ------------------------------------------------
        // TEST 4: Partial transfers of 1, 2 and 3 bytes.
        // Each test erases the sector before programming.
        // ------------------------------------------------
        erase_flash(0, 4);
        words[0] = 0x000000A5u;
        program_flash(0, 1, words, 1);
        read_flash(0, 1, &value, 1);
        assert((value & 0xFFu) == 0xA5u);
        cout << "TEST 4a PASS: 1-byte transfer\n";

        erase_flash(0, 4);
        words[0] = 0x0000BEEFu;
        program_flash(0, 2, words, 1);
        read_flash(0, 2, &value, 1);
        assert((value & 0xFFFFu) == 0xBEEFu);
        cout << "TEST 4b PASS: 2-byte transfer\n";

        erase_flash(0, 4);
        words[0] = 0x00ABCDEFu;
        program_flash(0, 3, words, 1);
        read_flash(0, 3, &value, 1);
        assert((value & 0x00FFFFFFu) == 0x00ABCDEFu);
        cout << "TEST 4c PASS: 3-byte transfer\n";

        // ------------------------------------------------
        // TEST 5: Five-byte streaming transfer.
        // First word: 11 22 33 44
        // Second word: 55 (only one byte is valid)
        // ------------------------------------------------
        erase_flash(0, 5);

        words[0] = 0x44332211u;
        words[1] = 0x00000055u;

        program_flash(0, 5, words, 2);
        read_flash(0, 5, words, 2);

        assert(words[0] == 0x44332211u);
        assert((words[1] & 0xFFu) == 0x55u);

        cout << "TEST 5 PASS: 5-byte streaming transfer\n";

        // ------------------------------------------------
        // TEST 6: W1C DONE status behavior.
        // Write 1 to DONE to clear it.
        // ------------------------------------------------
        assert(bus.read(STATUS, value));
        assert((value & DONE_BIT) != 0);

        assert(bus.write(STATUS, DONE_BIT));
        assert(bus.read(STATUS, value));

        assert((value & DONE_BIT) == 0);
        assert((value & ERROR_BIT) == 0);

        cout << "TEST 6 PASS: DONE write-one-to-clear\n";

        // ------------------------------------------------
        // TEST 7: Invalid address beyond the 1 MiB flash.
        // This implementation is expected to set ERROR.
        // ------------------------------------------------
        assert(bus.write(ADDR, 1u * 1024u * 1024u));
        assert(bus.write(LEN, 4));
        assert(bus.write(CTRL, START_BIT | 2u));

        value = wait_for_completion();

        assert((value & ERROR_BIT) != 0);
        assert((value & DONE_BIT) == 0);

        cout << "TEST 7 PASS: Out-of-range address rejected\n";

        // ------------------------------------------------
        // TEST 8: Range starts inside flash but extends
        // beyond its end.
        // ------------------------------------------------
        assert(bus.write(ADDR, (1u * 1024u * 1024u) - 2u));
        assert(bus.write(LEN, 4));
        assert(bus.write(CTRL, START_BIT | 2u));

        value = wait_for_completion();

        assert((value & ERROR_BIT) != 0);
        assert((value & DONE_BIT) == 0);

        cout << "TEST 8 PASS: Out-of-range length rejected\n";

        // ------------------------------------------------
        // TEST 9: Check that register addresses are mapped.
        // Bus returns false when no registered slave owns
        // the address.
        // ------------------------------------------------
        value = 0x12345678u;

        assert(!bus.read(0x200004FC, value));

        cout << "TEST 9 PASS: Unmapped address rejected by bus\n";

        // ------------------------------------------------
        // Final result
        // ------------------------------------------------
        cout << "\nALL SELECTED FLASH TESTS PASSED!\n";
        cout << "Review any specification-specific cases "
             << "before integration.\n";

        sc_stop();
    }
};

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    Bus bus;

    FlashInterface flash_if("flash_if", bus, "flash.img");

    sc_signal<bool> fi_done_sig("fi_done_sig");
    sc_signal<bool> fi_error_sig("fi_error_sig");

    flash_if.fi_done_out(fi_done_sig);
    flash_if.fi_error_out(fi_error_sig);

    FlashTestbench tb("tb", bus);

    tb.fi_done(fi_done_sig);
    tb.fi_error(fi_error_sig);

    sc_start();

    return 0;
}



// #include <systemc>
// #include <cstdint>
// #include <iostream>
// #include <cassert>

// #include "bus.h"
// #include "flash.h"

// using namespace sc_core;
// using namespace std;

// //====================================================
// // Flash Testbench
// //====================================================
// SC_MODULE(FlashTestbench)
// {
//     // Use the bus created in sc_main().
//     Bus& bus;

//     // Observe Flash Interface output signals.
//     sc_in<bool> fi_done;
//     sc_in<bool> fi_error;

//     SC_HAS_PROCESS(FlashTestbench);

//     FlashTestbench(sc_module_name name, Bus& bus)
//         : sc_module(name),
//           bus(bus),
//           fi_done("fi_done"),
//           fi_error("fi_error")
//     {
//         SC_THREAD(run_test);
//     }

    
//     void test_partial_length(uint32_t length, uint32_t expected)
//     {
//         const uint32_t CTRL = 0x20000400;
//         const uint32_t ADDR = 0x20000404;
//         const uint32_t LEN  = 0x20000408;
//         const uint32_t DATA = 0x20000410;

//         uint32_t value = 0;
//         bool completed = false;

//         // Erase sector zero first, so the test starts clean.
//         assert(bus.write(ADDR, 0));
//         assert(bus.write(LEN, 1));
//         assert(bus.write(CTRL, (1u << 8) | 3u));

//         for (unsigned int i = 0; i < 100; ++i)
//         {
//             wait(1, SC_NS);
//             assert(bus.read(0x2000040C, value));

//             if ((value & 1u) == 0)
//             {
//                 completed = true;
//                 break;
//             }
//         }

//         assert(completed && "ERASE timed out");

//         // Configure PROGRAM for the requested byte length.
//         assert(bus.write(ADDR, 0));
//         assert(bus.write(LEN, length));
//         assert(bus.write(CTRL, (1u << 8) | 1u));

//         wait(SC_ZERO_TIME);

//         // Supply enough 32-bit words for the requested length.
//         for (uint32_t offset = 0; offset < length; offset += 4)
//         {
//             assert(bus.write(DATA, expected >> (offset * 8)));
//         }

//         // Wait for PROGRAM to finish.
//         completed = false;

//         for (unsigned int i = 0; i < 100; ++i)
//         {
//             wait(1, SC_NS);
//             assert(bus.read(0x2000040C, value));

//             if ((value & 1u) == 0)
//             {
//                 completed = true;
//                 break;
//             }
//         }

//         assert(completed && "PROGRAM timed out");

//         // Start READ.
//         assert(bus.write(ADDR, 0));
//         assert(bus.write(LEN, length));
//         assert(bus.write(CTRL, (1u << 8) | 2u));

//         wait(SC_ZERO_TIME);

//         assert(bus.read(DATA, value));

//         // Mask to the number of bytes requested.
//         uint32_t mask = (length == 4)
//             ? 0xFFFFFFFFu
//             : ((1u << (length * 8u)) - 1u);

//         cout << "Length " << length
//             << ": read 0x" << hex << value << dec << endl;

//         assert((value & mask) == (expected & mask));

//         cout << "Partial-length test passed." << endl;
//     }


//     void run_test()
//     {
//         // Flash register addresses.
//         const uint32_t CTRL   = 0x20000400;
//         const uint32_t ADDR   = 0x20000404;
//         const uint32_t LEN    = 0x20000408;
//         const uint32_t STATUS = 0x2000040C;
//         const uint32_t DATA   = 0x20000410;

//         uint32_t value = 0;
//         bool completed = false;

//         cout << "Starting Flash Interface test..." << endl;

//         // Allow SystemC initialization to settle.
//         wait(SC_ZERO_TIME);

//         // Clean the first sector before starting the tests.
//         assert(bus.write(ADDR, 0x00000000));
//         assert(bus.write(LEN, 4));
//         assert(bus.write(CTRL, (1u << 8) | 3u)); // ERASE

//         // Wait for ERASE to complete.
//         completed = false;

//         for (unsigned int i = 0; i < 100; ++i)
//         {
//             wait(1, SC_NS);
//             assert(bus.read(STATUS, value));

//             if ((value & 0x1u) == 0)
//             {
//                 completed = true;
//                 break;
//             }
//         }

//         assert(completed && "Initial ERASE timed out");

//         //--------------------------------------------
//         // TEST 1: PROGRAM four bytes
//         //--------------------------------------------
//         assert(bus.write(ADDR, 0x00000000));
//         assert(bus.write(LEN, 4));

//         // OP = 1 (PROGRAM), START = bit 8.
//         assert(bus.write(CTRL, (1u << 8) | 1u));

//         // Allow the Flash operation thread to start.
//         wait(SC_ZERO_TIME);

//         // Program 0x12345678 at flash address zero.
//         assert(bus.write(DATA, 0x12345678));

//         // Wait for BUSY to clear, with a timeout.
//         completed = false;

//         for (unsigned int i = 0; i < 100; ++i)
//         {
//             wait(1, SC_NS);

//             assert(bus.read(STATUS, value));

//             if ((value & 0x1u) == 0)
//             {
//                 completed = true;
//                 break;
//             }
//         }

//         assert(completed && "PROGRAM timed out");

//         assert(bus.read(STATUS, value));

//         cout << "PROGRAM STATUS = 0x"
//              << hex << value << dec << endl;

//         assert((value & 0x2u) != 0); // DONE must be set.
//         assert((value & 0x4u) == 0); // ERROR must be clear.

//         cout << "Test 1 passed: PROGRAM" << endl;

//         //--------------------------------------------
//         // TEST 2: READ the programmed data
//         //--------------------------------------------
//         assert(bus.write(ADDR, 0x00000000));
//         assert(bus.write(LEN, 4));

//         // OP = 2 (READ), START = bit 8.
//         assert(bus.write(CTRL, (1u << 8) | 2u));

//         // Allow the operation thread to process START.
//         wait(SC_ZERO_TIME);

//         // Read the four bytes through FI_DATA.
//         assert(bus.read(DATA, value));

//         cout << "Read data = 0x"
//              << hex << value << dec << endl;

//         assert(value == 0x12345678);

//         // Wait for READ to complete.
//         completed = false;

//         for (unsigned int i = 0; i < 100; ++i)
//         {
//             assert(bus.read(STATUS, value));

//             if ((value & 0x1u) == 0)
//             {
//                 completed = true;
//                 break;
//             }

//             wait(1, SC_NS);
//         }

//         assert(completed && "READ timed out");

//         assert(bus.read(STATUS, value));

//         assert((value & 0x2u) != 0); // DONE must be set.
//         assert((value & 0x4u) == 0); // ERROR must be clear.

//         cout << "Test 2 passed: READ" << endl;

        
//         //--------------------------------------------
//         // TEST 3: ERASE the first flash sector
//         //--------------------------------------------

//         // Set flash address to zero.
//         assert(bus.write(ADDR, 0x00000000));

//         // Request erase of the sector containing address zero.
//         assert(bus.write(LEN, 4));

//         // OP = 3 (ERASE), START = bit 8.
//         assert(bus.write(CTRL, (1u << 8) | 3u));

//         // Wait for BUSY to clear, with a timeout.
//         completed = false;

//         for (unsigned int i = 0; i < 100; ++i)
//         {
//             wait(1, SC_NS);

//             assert(bus.read(STATUS, value));

//             if ((value & 0x1u) == 0)
//             {
//                 completed = true;
//                 break;
//             }
//         }

//         assert(completed && "ERASE timed out");

//         assert(bus.read(STATUS, value));

//         cout << "ERASE STATUS = 0x"
//              << hex << value << dec << endl;

//         assert((value & 0x2u) != 0); // DONE must be set.
//         assert((value & 0x4u) == 0); // ERROR must be clear.

//         cout << "Test 3 passed: ERASE operation completed" << endl;

//         //--------------------------------------------
//         // Verify erased data using READ
//         //--------------------------------------------

//         assert(bus.write(ADDR, 0x00000000));
//         assert(bus.write(LEN, 4));

//         // OP = 2 (READ), START = bit 8.
//         assert(bus.write(CTRL, (1u << 8) | 2u));

//         wait(SC_ZERO_TIME);

//         assert(bus.read(DATA, value));

//         cout << "Data after erase = 0x"
//              << hex << value << dec << endl;

//         assert(value == 0xFFFFFFFFu);

//         cout << "Test 4 passed: erased bytes read as 0xFF" << endl;
        
//         //--------------------------------------------
//         // TEST 5: Partial-word PROGRAM (5 bytes)
//         //--------------------------------------------

//         // Start from an erased flash region.
//         assert(bus.write(ADDR, 0x00000000));
//         assert(bus.write(LEN, 5));

//         // OP = 1 (PROGRAM), START = bit 8.
//         assert(bus.write(CTRL, (1u << 8) | 1u));

//         wait(SC_ZERO_TIME);

//         // First word: 4 valid bytes (11 22 33 44).
//         assert(bus.write(DATA, 0x44332211));

//         // Second word: only the lowest byte (55) is valid.
//         assert(bus.write(DATA, 0x00000055));

//         // Wait for PROGRAM completion.
//         completed = false;

//         for (unsigned int i = 0; i < 100; ++i)
//         {
//             wait(1, SC_NS);
//             assert(bus.read(STATUS, value));

//             if ((value & 0x1u) == 0)
//             {
//                 completed = true;
//                 break;
//             }
//         }

//         assert(completed && "Partial PROGRAM timed out");

//         assert(bus.read(STATUS, value));
//         assert((value & 0x2u) != 0);
//         assert((value & 0x4u) == 0);

//         cout << "Test 5 passed: 5-byte PROGRAM" << endl;

//         //--------------------------------------------
//         // TEST 6: Partial-word READ (5 bytes)
//         //--------------------------------------------

//         assert(bus.write(ADDR, 0x00000000));
//         assert(bus.write(LEN, 5));

//         // OP = 2 (READ), START = bit 8.
//         assert(bus.write(CTRL, (1u << 8) | 2u));

//         wait(SC_ZERO_TIME);

//         // Read first 4 bytes.
//         assert(bus.read(DATA, value));
//         cout << "First word  = 0x"
//              << hex << value << dec << endl;
//         assert(value == 0x44332211);

//         // Read the remaining 1 byte.
//         assert(bus.read(DATA, value));
//         cout << "Second word = 0x"
//              << hex << value << dec << endl;
//         assert(value == 0x00000055);

//         // Wait for READ completion.
//         completed = false;

//         for (unsigned int i = 0; i < 100; ++i)
//         {
//             assert(bus.read(STATUS, value));

//             if ((value & 0x1u) == 0)
//             {
//                 completed = true;
//                 break;
//             }

//             wait(1, SC_NS);
//         }

//         assert(completed && "Partial READ timed out");

//         assert(bus.read(STATUS, value));
//         assert((value & 0x2u) != 0);
//         assert((value & 0x4u) == 0);

//         cout << "Test 6 passed: 5-byte READ" << endl;

//         test_partial_length(1, 0x000000A5);
//         test_partial_length(2, 0x0000BEEF);
//         test_partial_length(3, 0x00ABCDEF);

//         //--------------------------------------------
//         // Final result
//         //--------------------------------------------
//         cout << "All Flash tests passed!" << endl;

//         sc_stop();
//     }
// };

// //====================================================
// // Simulation entry point
// //====================================================
// int sc_main(int argc, char* argv[])
// {
//     (void)argc;
//     (void)argv;

//     // 1. Create the shared bus.
//     Bus bus;

//     // 2. Create the Flash Interface DUT.
//     FlashInterface flash_if("flash_if", bus, "flash.img");

//     // 3. Create signals for DUT outputs.
//     sc_signal<bool> fi_done_sig("fi_done_sig");
//     sc_signal<bool> fi_error_sig("fi_error_sig");

//     // 4. Connect DUT output ports.
//     flash_if.fi_done_out(fi_done_sig);
//     flash_if.fi_error_out(fi_error_sig);

//     // 5. Create the testbench using the same bus.
//     FlashTestbench tb("tb", bus);

//     // 6. Connect testbench input ports.
//     tb.fi_done(fi_done_sig);
//     tb.fi_error(fi_error_sig);

//     // 7. Run the simulation.
//     sc_start();

//     return 0;
// }
