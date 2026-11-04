#include <systemc>
#include <cstdint>
#include <iostream>
#include <cassert>
#include "irq_ctrl.h"

SC_MODULE(IRQ_Testbench){
    
    Bus& bus;

    // Signals driven by the testbench.
    sc_core::sc_out<bool> hi_done;
    sc_core::sc_out<bool> hi_error;
    sc_core::sc_out<bool> fi_done;
    sc_core::sc_out<bool> fi_error;
    sc_core::sc_out<bool> cp_done;
    sc_core::sc_out<bool> cp_error;

    // Observe the controller's interrupt output.
    sc_core::sc_in<bool> irq;

    SC_HAS_PROCESS(IRQ_Testbench);

    IRQ_Testbench(sc_core::sc_module_name name, Bus& bus)
        : sc_core::sc_module(name), 
          bus(bus),
          hi_done("hi_done"),
          hi_error("hi_error"),
          fi_done("fi_done"),
          fi_error("fi_error"),
          cp_done("cp_done"),
          cp_error("cp_error"),
          irq("irq")
    {
        SC_THREAD(run_test);
    }

    void run_test() 
    {
        // Drive all interrupt inputs low.
        hi_done.write(false);
        hi_error.write(false);
        fi_done.write(false);
        fi_error.write(false);
        cp_done.write(false);
        cp_error.write(false);

        // Allow signal updates and initialization to settle.
        wait(sc_core::SC_ZERO_TIME);

        std::cout << "Testbench initialized\n";

        uint32_t value = 0;

        assert(bus.read(0x20000300, value));
        assert(value == 0);

        assert(bus.read(0x20000304, value));
        assert(value == 0);

        assert(!irq.read());

        std::cout << "Test 1 passed: reset state\n";

        // Test 2: Raise sets pending bits
        assert(bus.write(0x2000030c, 0x0b)); // Raise bits 0, 1, and 3
        assert(bus.read(0x20000300, value));
        assert(value == 0x0b);

        std::cout << "Test 2 passed: raise sets pending bits\n";

        // Test 3: Enable only interrupt bit 0.
        assert(bus.write(0x20000304, 0x01));

        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        assert(irq.read());

        std::cout << "Test 3 passed: MASK enables IRQ\n";

        // Test 4: CLEAR bits 0 and 3.
        // PENDING was 0x0B; clearing 0x09 should leave 0x02.
        assert(bus.write(0x20000308, 0x09));

        assert(bus.read(0x20000300, value));
        assert(value == 0x02);

        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
        assert(!irq.read());

        std::cout << "Test 4 passed: CLEAR register\n";


        // Test 5: CLEAR and RAISE read as zero.
        assert(bus.read(0x20000308, value));
        assert(value == 0);

        assert(bus.read(0x2000030C, value));
        assert(value == 0);

        std::cout << "Test 5 passed: strobe registers read zero\n";


        // Test 6: PENDING is read-only.
        // Attempting to write 0x3F must not change PENDING = 0x02.
        assert(bus.write(0x20000300, 0x3F));

        assert(bus.read(0x20000300, value));
        assert(value == 0x02);

        std::cout << "Test 6 passed: PENDING is read-only\n";


        // Test 7: MASK stores only six valid bits.
        assert(bus.write(0x20000304, 0xFFFFFFFF));

        assert(bus.read(0x20000304, value));
        assert(value == 0x3F);

        std::cout << "Test 7 passed: MASK ignores reserved bits\n";


        // Test 8: Clear all pending bits and disable all masks.
        assert(bus.write(0x20000308, 0x3F));
        assert(bus.write(0x20000304, 0x00));

        wait(sc_core::SC_ZERO_TIME);

        assert(bus.read(0x20000300, value));
        assert(value == 0x00);
        assert(!irq.read());

        std::cout << "Test 8 passed: clear all and disable IRQ\n";


        // Test 9: hi_done_in sets PENDING bit 0.
        hi_done.write(false);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        assert(bus.write(0x20000304, 0x01));

        hi_done.write(true);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        assert(bus.read(0x20000300, value));
        assert(value == 0x01);
        assert(irq.read());

        std::cout << "Test 9 passed: hi_done interrupt\n";


        // Test 10: Falling edge must not clear PENDING.
        hi_done.write(false);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        assert(bus.read(0x20000300, value));
        assert(value == 0x01);
        assert(irq.read());

        std::cout << "Test 10 passed: pending survives falling edge\n";


        // Clear bit 0 before testing the other inputs.
        assert(bus.write(0x20000308, 0x3F));
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
        assert(!irq.read());


        // Test 11: hi_error_in sets bit 1.
        hi_error.write(true);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        assert(bus.read(0x20000300, value));
        assert(value == 0x02);

        std::cout << "Test 11 passed: hi_error interrupt\n";

        hi_error.write(false);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        // Clear all pending bits before the next independent test.
        assert(bus.write(0x20000308, 0x3F));
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        assert(bus.read(0x20000300, value));
        assert(value == 0x00);

        // Test 12: fi_done_in sets bit 2.
        fi_done.write(true);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        assert(bus.read(0x20000300, value));
        assert(value == 0x04);

        std::cout << "Test 12 passed: fi_done interrupt\n";

        // Clear all pending bits before the next independent test.
        assert(bus.write(0x20000308, 0x3F));
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        assert(bus.read(0x20000300, value));
        assert(value == 0x00);

        fi_done.write(false);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);


        // Test 13: fi_error_in sets bit 3.
        fi_error.write(true);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        assert(bus.read(0x20000300, value));
        assert(value == 0x08);

        std::cout << "Test 13 passed: fi_error interrupt\n";

        fi_error.write(false);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        // Clear all pending bits before the next independent test.
        assert(bus.write(0x20000308, 0x3F));
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        assert(bus.read(0x20000300, value));
        assert(value == 0x00);

        // Test 14: cp_done_in sets bit 4.
        cp_done.write(true);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        assert(bus.read(0x20000300, value));
        assert(value == 0x10);

        std::cout << "Test 14 passed: cp_done interrupt\n";

        cp_done.write(false);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        // Clear all pending bits before the next independent test.
        assert(bus.write(0x20000308, 0x3F));
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        assert(bus.read(0x20000300, value));
        assert(value == 0x00);
        
        // Test 15: cp_error_in sets bit 5.
        cp_error.write(true);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        assert(bus.read(0x20000300, value));
        assert(value == 0x20);

        std::cout << "Test 15 passed: cp_error interrupt\n";

        cp_error.write(false);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);


        // Test 16: Clear all pending bits.
        assert(bus.write(0x20000308, 0x3F));

        assert(bus.read(0x20000300, value));
        assert(value == 0x00);

        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
        assert(!irq.read());

        std::cout << "Test 16 passed: clear all pending bits\n";

        std::cout << "All IRQ tests passed!\n";
        sc_core::sc_stop();
    }
};

int sc_main(int argc, char* argv[])
{
    Bus bus;

    Interrupt_Controller irq_ctrl("irq_ctrl", bus);
    IRQ_Testbench tb("tb", bus);
    // Declare seven sc_signal<bool> objects here:
    // six interrupt signals and one IRQ output signal.
    
    sc_core::sc_signal<bool> hi_done_sig("hi_done_sig");
    sc_core::sc_signal<bool> hi_error_sig("hi_error_sig");
    sc_core::sc_signal<bool> fi_done_sig("fi_done_sig");
    sc_core::sc_signal<bool> fi_error_sig("fi_error_sig");
    sc_core::sc_signal<bool> cp_done_sig("cp_done_sig");
    sc_core::sc_signal<bool> cp_error_sig("cp_error_sig");

    sc_core::sc_signal<bool> irq_out_sig("irq_out_sig");

    // Bind the controller's six inputs to the six signals.
    // Bind the controller's irq_out to the IRQ signal.
    
    irq_ctrl.hi_done_in(hi_done_sig);
    irq_ctrl.hi_error_in(hi_error_sig);
    irq_ctrl.fi_done_in(fi_done_sig);
    irq_ctrl.fi_error_in(fi_error_sig);
    irq_ctrl.cp_done_in(cp_done_sig);
    irq_ctrl.cp_error_in(cp_error_sig);
    irq_ctrl.irq_out(irq_out_sig);

    // Bind the testbench's six outputs to the six signals.
    // Bind the testbench's irq to the IRQ signal.

    tb.hi_done(hi_done_sig);
    tb.hi_error(hi_error_sig);
    tb.fi_done(fi_done_sig);
    tb.fi_error(fi_error_sig);
    tb.cp_done(cp_done_sig);
    tb.cp_error(cp_error_sig);
    tb.irq(irq_out_sig);

    sc_core::sc_start();

    return 0;
}
    
