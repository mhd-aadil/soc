
#include <systemc>
#include <iostream>
#include <cassert>

#include "bus.h"
#include "flash.h"

using namespace sc_core;
using namespace std;

SC_MODULE(FlashTest)
{
    Bus bus;
    FlashInterface* flash_if;

    sc_signal<bool> fi_done;
    sc_signal<bool> fi_error;

    static constexpr uint32_t CTRL   = 0x20000400;
    static constexpr uint32_t ADDR   = 0x20000404;
    static constexpr uint32_t LEN    = 0x20000408;
    static constexpr uint32_t STATUS = 0x2000040C;
    static constexpr uint32_t DATA   = 0x20000410;

    SC_CTOR(FlashTest)
    {
        flash_if = new FlashInterface("flash_if", bus, "flash.img");

        flash_if->fi_done_out(fi_done);
        flash_if->fi_error_out(fi_error);

        SC_THREAD(run_test);
    }

    ~FlashTest()
    {
        delete flash_if;
    }

    void run_test()
    {
        uint32_t value = 0;

        cout << "Starting Flash Interface test..." << endl;

        // Allow SystemC initialization to finish.
        wait(SC_ZERO_TIME);

        // Configure the PROGRAM operation.
        assert(bus.write(ADDR, 0x00000000));
        assert(bus.write(LEN, 4));

        // OP = 1 (PROGRAM), START = bit 8.
        assert(bus.write(CTRL, (1u << 8) | 1u));

        // Allow the Flash Interface operation thread to start.
        wait(SC_ZERO_TIME);

        // Program four bytes.
        assert(bus.write(DATA, 0x12345678));

        // Wait for the operation to finish.
        while (true)
        {
            assert(bus.read(STATUS, value));

            if ((value & 0x1u) == 0)
                break;

            wait(1, SC_NS);
        }

        assert(bus.read(STATUS, value));
        cout << "PROGRAM STATUS = 0x" << hex << value << dec << endl;

        assert((value & 0x2u) != 0);  // DONE must be set.
        assert((value & 0x4u) == 0);  // ERROR must be clear.

        // Configure the READ operation.
        assert(bus.write(ADDR, 0x00000000));
        assert(bus.write(LEN, 4));
        assert(bus.write(CTRL, (1u << 8) | 2u));

        // Let the operation thread process START.
        wait(SC_ZERO_TIME);

        // Read the programmed data word.
        assert(bus.read(DATA, value));

        cout << "Read data = 0x" << hex << value << dec << endl;
        assert(value == 0x12345678);

        // Wait for READ to finish.
        while (true)
        {
            assert(bus.read(STATUS, value));

            if ((value & 0x1u) == 0)
                break;

            wait(1, SC_NS);
        }

        assert(bus.read(STATUS, value));
        assert((value & 0x2u) != 0);
        assert((value & 0x4u) == 0);

        cout << "PROGRAM/READ test passed." << endl;
        sc_stop();
    }
};

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    FlashTest test("test");
    sc_start();

    cout << "Simulation completed." << endl;
    cout << "fi_done  = " << test.fi_done.read() << endl;
    cout << "fi_error = " << test.fi_error.read() << endl;

    return 0;
}
