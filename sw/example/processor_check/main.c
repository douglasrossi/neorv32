// #################################################################################################
// # << NEORV32 - Processor Test Program >>                                                        #
// # ********************************************************************************************* #
// # BSD 3-Clause License                                                                          #
// #                                                                                               #
// # Copyright (c) 2021, Stephan Nolting. All rights reserved.                                     #
// #                                                                                               #
// # Redistribution and use in source and binary forms, with or without modification, are          #
// # permitted provided that the following conditions are met:                                     #
// #                                                                                               #
// # 1. Redistributions of source code must retain the above copyright notice, this list of        #
// #    conditions and the following disclaimer.                                                   #
// #                                                                                               #
// # 2. Redistributions in binary form must reproduce the above copyright notice, this list of     #
// #    conditions and the following disclaimer in the documentation and/or other materials        #
// #    provided with the distribution.                                                            #
// #                                                                                               #
// # 3. Neither the name of the copyright holder nor the names of its contributors may be used to  #
// #    endorse or promote products derived from this software without specific prior written      #
// #    permission.                                                                                #
// #                                                                                               #
// # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS   #
// # OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF               #
// # MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE    #
// # COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,     #
// # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE #
// # GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED    #
// # AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING     #
// # NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED  #
// # OF THE POSSIBILITY OF SUCH DAMAGE.                                                            #
// # ********************************************************************************************* #
// # The NEORV32 Processor - https://github.com/stnolting/neorv32              (c) Stephan Nolting #
// #################################################################################################


/**********************************************************************//**
 * @file processor_check/main.c
 * @author Stephan Nolting
 * @brief CPU/Processor test program.
 **************************************************************************/

#include <neorv32.h>
#include <string.h>


/**********************************************************************//**
 * @name User configuration
 **************************************************************************/
/**@{*/
/** UART BAUD rate */
#define BAUD_RATE           (19200)
//** Reachable unaligned address */
#define ADDR_UNALIGNED      (0x00000002)
//** Unreachable word-aligned address */
#define ADDR_UNREACHABLE    (IO_BASE_ADDRESS-4)
//** external memory base address */
#define EXT_MEM_BASE        (0xF0000000)
/**@}*/


/**********************************************************************//**
 * @name UART print macros
 **************************************************************************/
/**@{*/
//** for simulation only! */
#ifdef SUPPRESS_OPTIONAL_UART_PRINT
//** print standard output to UART0 */
#define PRINT_STANDARD(...) 
//** print critical output to UART1 */
#define PRINT_CRITICAL(...) neorv32_uart1_printf(__VA_ARGS__)
#else
//** print standard output to UART0 */
#define PRINT_STANDARD(...) neorv32_uart0_printf(__VA_ARGS__)
//** print critical output to UART0 */
#define PRINT_CRITICAL(...) neorv32_uart0_printf(__VA_ARGS__)
#endif
/**@}*/


// Prototypes
void sim_irq_trigger(uint32_t sel);
void global_trap_handler(void);
void xirq_trap_handler0(void);
void xirq_trap_handler1(void);
void test_ok(void);
void test_fail(void);

// Global variables (also test initialization of global vars here)
/// Global counter for failing tests
int cnt_fail = 0;
/// Global counter for successful tests
int cnt_ok   = 0;
/// Global counter for total number of tests
int cnt_test = 0;
/// Global number of available HPMs
uint32_t num_hpm_cnts_global = 0;
/// XIRQ trap handler acknowledge
uint32_t xirq_trap_handler_ack = 0;

/// Variable to test atomic accesses
uint32_t atomic_access_addr;


/**********************************************************************//**
 * High-level CPU/processor test program.
 *
 * @note Applications has to be compiler with <USER_FLAGS+=-DRUN_CPUTEST>
 * @warning This test is intended for simulation only.
 * @warning This test requires all optional extensions/modules to be enabled.
 *
 * @return 0 if execution was successful
 **************************************************************************/
int main() {

  register uint32_t tmp_a, tmp_b;
  uint8_t id;


  // init UARTs at default baud rate, no parity bits, no hw flow control
  neorv32_uart0_setup(BAUD_RATE, PARITY_NONE, FLOW_CONTROL_NONE);
  UART1_CT = UART0_CT; // copy configuration to initialize UART1

#ifdef SUPPRESS_OPTIONAL_UART_PRINT
  neorv32_uart0_disable(); // do not generate any UART0 output
#endif

// Disable processor_check compilation by default
#ifndef RUN_CHECK
  #warning processor_check HAS NOT BEEN COMPILED! Use >>make USER_FLAGS+=-DRUN_CHECK clean_all exe<< to compile it.

  // inform the user if you are actually executing this
  PRINT_CRITICAL("ERROR! processor_check has not been compiled. Use >>make USER_FLAGS+=-DRUN_CHECK clean_all exe<< to compile it.\n");

  return 1;
#endif


  // ----------------------------------------------
  // setup RTE
  neorv32_rte_setup(); // this will install a full-detailed debug handler for ALL traps
  // ----------------------------------------------

  // check available hardware extensions and compare with compiler flags
  neorv32_rte_check_isa(0); // silent = 0 -> show message if isa mismatch


  // intro
  PRINT_STANDARD("\n<< PROCESSOR CHECK >>\n");
  PRINT_STANDARD("build: "__DATE__" "__TIME__"\n");


  // reset performance counter
  neorv32_cpu_csr_write(CSR_MCYCLEH, 0);
  neorv32_cpu_csr_write(CSR_MCYCLE, 0);
  neorv32_cpu_csr_write(CSR_MINSTRETH, 0);
  neorv32_cpu_csr_write(CSR_MINSTRET, 0);
  neorv32_cpu_csr_write(CSR_MCOUNTINHIBIT, 0); // enable performance counter auto increment (ALL counters)
  neorv32_cpu_csr_write(CSR_MCOUNTEREN, 7); // allow access from user-mode code to standard counters only

  neorv32_mtime_set_time(0);
  // set CMP of machine system timer MTIME to max to prevent an IRQ
  uint64_t mtime_cmp_max = 0xffffffffffffffffULL;
  neorv32_mtime_set_timecmp(mtime_cmp_max);


  // fancy intro
  // -----------------------------------------------
  // logo
  neorv32_rte_print_logo();

  // show project credits
  neorv32_rte_print_credits();

  // show full HW config report
  neorv32_rte_print_hw_config();


  // configure RTE
  // -----------------------------------------------
  PRINT_STANDARD("\n\nConfiguring NEORV32 RTE... ");

  int install_err = 0;
  // initialize ALL provided trap handler (overriding the default debug handlers)
  for (id=0; id<NEORV32_RTE_NUM_TRAPS; id++) {
    install_err += neorv32_rte_exception_install(id, global_trap_handler);
  }

  if (install_err) {
    PRINT_CRITICAL("RTE install error (%i)!\n", install_err);
    return 1;
  }

  // enable interrupt sources
  neorv32_cpu_irq_enable(CSR_MIE_MSIE); // machine software interrupt
  neorv32_cpu_irq_enable(CSR_MIE_MTIE); // machine timer interrupt
  neorv32_cpu_irq_enable(CSR_MIE_MEIE); // machine external interrupt
  // enable FAST IRQ sources only where actually needed

  // test intro
  PRINT_STANDARD("\nStarting tests...\n\n");

  // enable global interrupts
  neorv32_cpu_eint();


  // ----------------------------------------------------------
  // Test standard RISC-V performance counter [m]cycle[h]
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] [m]cycle[h] counter: ", cnt_test);

  cnt_test++;

  // make sure counter is enabled
  asm volatile ("csrci %[addr], %[imm]" : : [addr] "i" (CSR_MCOUNTINHIBIT), [imm] "i" (1<<CSR_MCOUNTINHIBIT_CY));

  // prepare overflow
  neorv32_cpu_set_mcycle(0x00000000FFFFFFFFULL);

  // get current cycle counter HIGH
  tmp_a = neorv32_cpu_csr_read(CSR_MCYCLEH);

  // make sure cycle counter high has incremented and there was no exception during access
  if ((tmp_a == 1) && (neorv32_cpu_csr_read(CSR_MCAUSE) == 0)) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Test standard RISC-V performance counter [m]instret[h]
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] [m]instret[h] counter: ", cnt_test);

  cnt_test++;

  // make sure counter is enabled
  asm volatile ("csrci %[addr], %[imm]" : : [addr] "i" (CSR_MCOUNTINHIBIT), [imm] "i" (1<<CSR_MCOUNTINHIBIT_IR));

  // prepare overflow
  neorv32_cpu_set_minstret(0x00000000FFFFFFFFULL);

  // get instruction counter HIGH
  tmp_a = neorv32_cpu_csr_read(CSR_INSTRETH);

  // make sure instruction counter high has incremented and there was no exception during access
  if ((tmp_a == 1) && (neorv32_cpu_csr_read(CSR_MCAUSE) == 0)) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Test mcountinhibt: inhibit auto-inc of [m]cycle
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] mcountinhibt.cy CSR: ", cnt_test);

  cnt_test++;

  // inhibit [m]cycle CSR
  tmp_a = neorv32_cpu_csr_read(CSR_MCOUNTINHIBIT);
  tmp_a |= (1<<CSR_MCOUNTINHIBIT_CY); // inhibit cycle counter auto-increment
  neorv32_cpu_csr_write(CSR_MCOUNTINHIBIT, tmp_a);

  // get current cycle counter
  tmp_a = neorv32_cpu_csr_read(CSR_CYCLE);

  // wait some time to have a nice "increment" (there should be NO increment at all!)
  asm volatile ("nop");
  asm volatile ("nop");

  tmp_b = neorv32_cpu_csr_read(CSR_CYCLE);

  // make sure instruction counter has NOT incremented and there was no exception during access
  if ((tmp_a == tmp_b) && (tmp_a != 0) && (neorv32_cpu_csr_read(CSR_MCAUSE) == 0)) {
    test_ok();
  }
  else {
    test_fail();
  }

  // re-enable [m]cycle CSR
  tmp_a = neorv32_cpu_csr_read(CSR_MCOUNTINHIBIT);
  tmp_a &= ~(1<<CSR_MCOUNTINHIBIT_CY); // clear inhibit of cycle counter auto-increment
  neorv32_cpu_csr_write(CSR_MCOUNTINHIBIT, tmp_a);


  // ----------------------------------------------------------
  // Test mcounteren: do not allow cycle[h] access from user-mode
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] mcounteren.cy CSR: ", cnt_test);

  // skip if U-mode is not implemented
  if (neorv32_cpu_csr_read(CSR_MISA) & (1<<CSR_MISA_U)) {
    cnt_test++;

    // do not allow user-level code to access cycle[h] CSRs
    tmp_a = neorv32_cpu_csr_read(CSR_MCOUNTEREN);
    tmp_a &= ~(1<<CSR_MCOUNTEREN_CY); // clear access right
    neorv32_cpu_csr_write(CSR_MCOUNTEREN, tmp_a);

    // switch to user mode (hart will be back in MACHINE mode when trap handler returns)
    neorv32_cpu_goto_user_mode();
    {
      // access to cycle CSR is no longer allowed
      tmp_a = neorv32_cpu_csr_read(CSR_CYCLE);
    }

    // make sure there was an illegal instruction trap
    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_I_ILLEGAL) {
      if (tmp_a == 0) { // make sure user-level code CANNOT read locked CSR content!
        test_ok();
      }
      else {
        test_fail();
      }
    }
    else {
      test_fail();
    }
  }
    else {
    PRINT_STANDARD("skipped (n.a.)\n");
  }


  // re-allow user-level code to access cycle[h] CSRs
  tmp_a = neorv32_cpu_csr_read(CSR_MCOUNTEREN);
  tmp_a |= (1<<CSR_MCOUNTEREN_CY); // re-allow access right
  neorv32_cpu_csr_write(CSR_MCOUNTEREN, tmp_a);


/*
  // ----------------------------------------------------------
  // Execute DRET in M-mode (has to trap!)
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] DRET in M-mode: ", cnt_test);

  // skip if U-mode is not implemented
  if (neorv32_cpu_csr_read(CSR_MISA) & (1<<CSR_MISA_U)) {

    cnt_test++;

    asm volatile("dret");

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_I_ILLEGAL) {
      test_ok();
    }
    else {
      test_fail();
    }

  }
  else {
    PRINT_STANDARD("skipped (n.a. without U-ext)\n");
  }
*/


/*
  // ----------------------------------------------------------
  // Execute MRET in U-mode (has to trap!)
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] MRET in U-mode: ", cnt_test);

  // skip if U-mode is not implemented
  if (neorv32_cpu_csr_read(CSR_MZEXT) & (1<<CSR_MZEXT_DEBUGMODE)) {

    cnt_test++;

    // switch to user mode (hart will be back in MACHINE mode when trap handler returns)
    neorv32_cpu_goto_user_mode();
    {
      asm volatile("mret");
    }

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_I_ILLEGAL) {
      test_ok();
    }
    else {
      test_fail();
    }

  }
  else {
    PRINT_STANDARD("skipped (n.a. without U-ext)\n");
  }
*/


  // ----------------------------------------------------------
  // Test performance counter: setup as many events and counter as feasible
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] Configuring HPM events: ", cnt_test);

  num_hpm_cnts_global = neorv32_cpu_hpm_get_counters();

  if (num_hpm_cnts_global != 0) {
    cnt_test++;

    neorv32_cpu_csr_write(CSR_MHPMCOUNTER3,  0); neorv32_cpu_csr_write(CSR_MHPMEVENT3,  1 << HPMCNT_EVENT_CIR);
    neorv32_cpu_csr_write(CSR_MHPMCOUNTER4,  0); neorv32_cpu_csr_write(CSR_MHPMEVENT4,  1 << HPMCNT_EVENT_WAIT_IF);
    neorv32_cpu_csr_write(CSR_MHPMCOUNTER5,  0); neorv32_cpu_csr_write(CSR_MHPMEVENT5,  1 << HPMCNT_EVENT_WAIT_II);
    neorv32_cpu_csr_write(CSR_MHPMCOUNTER6,  0); neorv32_cpu_csr_write(CSR_MHPMEVENT6,  1 << HPMCNT_EVENT_WAIT_MC);
    neorv32_cpu_csr_write(CSR_MHPMCOUNTER7,  0); neorv32_cpu_csr_write(CSR_MHPMEVENT7,  1 << HPMCNT_EVENT_LOAD);
    neorv32_cpu_csr_write(CSR_MHPMCOUNTER8,  0); neorv32_cpu_csr_write(CSR_MHPMEVENT8,  1 << HPMCNT_EVENT_STORE);
    neorv32_cpu_csr_write(CSR_MHPMCOUNTER9,  0); neorv32_cpu_csr_write(CSR_MHPMEVENT9,  1 << HPMCNT_EVENT_WAIT_LS);
    neorv32_cpu_csr_write(CSR_MHPMCOUNTER10, 0); neorv32_cpu_csr_write(CSR_MHPMEVENT10, 1 << HPMCNT_EVENT_JUMP);
    neorv32_cpu_csr_write(CSR_MHPMCOUNTER11, 0); neorv32_cpu_csr_write(CSR_MHPMEVENT11, 1 << HPMCNT_EVENT_BRANCH);
    neorv32_cpu_csr_write(CSR_MHPMCOUNTER12, 0); neorv32_cpu_csr_write(CSR_MHPMEVENT12, 1 << HPMCNT_EVENT_TBRANCH);
    neorv32_cpu_csr_write(CSR_MHPMCOUNTER13, 0); neorv32_cpu_csr_write(CSR_MHPMEVENT13, 1 << HPMCNT_EVENT_TRAP);
    neorv32_cpu_csr_write(CSR_MHPMCOUNTER14, 0); neorv32_cpu_csr_write(CSR_MHPMEVENT14, 1 << HPMCNT_EVENT_ILLEGAL);

    neorv32_cpu_csr_write(CSR_MCOUNTINHIBIT, 0); // enable all counters

    // make sure there was no exception
    if (neorv32_cpu_csr_read(CSR_MCAUSE) == 0) {
      test_ok();
    }
    else {
      test_fail();
    }
  }
  else {
    PRINT_STANDARD("skipped (n.a.)\n");
  }


  // ----------------------------------------------------------
  // External memory interface test
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] External memory access (@ 0x%x): ", cnt_test, (uint32_t)EXT_MEM_BASE);

  if (SYSINFO_FEATURES & (1 << SYSINFO_FEATURES_MEM_EXT)) {
    cnt_test++;

    // create test program in RAM
    static const uint32_t dummy_ext_program[2] __attribute__((aligned(8))) = {
      0x3407D073, // csrwi mscratch, 15
      0x00008067  // ret (32-bit)
    };

    // copy to external memory
    if (memcpy((void*)EXT_MEM_BASE, (void*)&dummy_ext_program, (size_t)sizeof(dummy_ext_program)) == NULL) {
      test_fail();
    }
    else {

      // execute program
      tmp_a = (uint32_t)EXT_MEM_BASE; // call the dummy sub program
      asm volatile ("jalr ra, %[input_i]" :  : [input_i] "r" (tmp_a));
    
      if (neorv32_cpu_csr_read(CSR_MCAUSE) == 0) { // make sure there was no exception
        if (neorv32_cpu_csr_read(CSR_MSCRATCH) == 15) { // make sure the program was executed in the right way
          test_ok();
        }
        else {
          test_fail();
        }
      }
      else {
        test_fail();
      }
    }
  }
  else {
    PRINT_STANDARD("skipped (n.a.)\n");
  }


  // ----------------------------------------------------------
  // Test FENCE.I instruction (instruction buffer / i-cache clear & reload)
  // if Zifencei is not implemented FENCE.I should execute as NOP
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] FENCE.I: ", cnt_test);

  cnt_test++;

  asm volatile ("fence.i");

  // make sure there was no exception (and that the cpu did not crash...)
  if (neorv32_cpu_csr_read(CSR_MCAUSE) == 0) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Illegal CSR access (CSR not implemented)
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] Non-existent CSR access: ", cnt_test);

  cnt_test++;

  tmp_a = neorv32_cpu_csr_read(0xfff); // CSR 0xfff not implemented

  if (tmp_a != 0) {
    PRINT_CRITICAL("%c[1m<SECURITY FAILURE> %c[0m\n", 27, 27);
  }

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_I_ILLEGAL) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Write-access to read-only CSR
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] Read-only CSR write access: ", cnt_test);

  cnt_test++;

  neorv32_cpu_csr_write(CSR_TIME, 0); // time CSR is read-only

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_I_ILLEGAL) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // No "real" CSR write access (because rs1 = r0)
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] Read-only CSR 'no-write' (rs1=0) access: ", cnt_test);

  cnt_test++;

  // time CSR is read-only, but no actual write is performed because rs1=r0
  // -> should cause no exception
  asm volatile("csrrs zero, time, zero");

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == 0) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Test pending interrupt
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] Pending IRQ test (MTIME): ", cnt_test);

  cnt_test++;

  // disable global interrupts
  neorv32_cpu_dint();

  // prepare MTIME IRQ
  neorv32_mtime_set_time(0x00000000FFFFFFF8ULL); // prepare overflow
  neorv32_mtime_set_timecmp(0x0000000100000000ULL); // IRQ on overflow

  // wait some time for the IRQ to arrive the CPU
  asm volatile("nop");
  asm volatile("nop");

  // no more mtime interrupts
  neorv32_mtime_set_timecmp(-1);

  // re-enable global interrupts
  neorv32_cpu_eint();


  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_MTI) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Unaligned instruction address
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] I_ALIGN (instr. alignment) EXC: ", cnt_test);

  // skip if C-mode is implemented
  if ((neorv32_cpu_csr_read(CSR_MISA) & (1<<CSR_MISA_C)) == 0) {

    cnt_test++;

    // call unaligned address
    ((void (*)(void))ADDR_UNALIGNED)();

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_I_MISALIGNED) {
      PRINT_STANDARD("ok\n");
      cnt_ok++;
    }
    else {
      PRINT_STANDARD("fail\n");
      cnt_fail++;
    }
  }
  else {
    PRINT_STANDARD("skipped (n.a. with C-ext)\n");
  }


  // ----------------------------------------------------------
  // Instruction access fault
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] I_ACC (instr. bus access) EXC: ", cnt_test);
  cnt_test++;

  // call unreachable aligned address
  ((void (*)(void))ADDR_UNREACHABLE)();

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_I_ACCESS) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Illegal instruction
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] I_ILLEG (illegal instr.) EXC: ", cnt_test);

  cnt_test++;

  asm volatile ("csrrw zero, 0xfff, zero"); // = 0xfff01073 : CSR 0xfff not implemented -> illegal instruction

  // make sure this has cause an illegal exception
  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_I_ILLEGAL) {
    // make sure this is really the instruction that caused the exception
    // for illegal instructions mtval contains the actual instruction word
    if (neorv32_cpu_csr_read(CSR_MTVAL) == 0xfff01073) {
      test_ok();
    }
    else {
      test_fail();
    }
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Illegal compressed instruction
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] CI_ILLEG (illegal compr. instr.) EXC: ", cnt_test);

  // skip if C-mode is not implemented
  if ((neorv32_cpu_csr_read(CSR_MISA) & (1<<CSR_MISA_C)) != 0) {

    cnt_test++;

    // create test program in RAM
    static const uint32_t dummy_sub_program_ci[2] __attribute__((aligned(8))) = {
      0x00000001, // 2nd: official_illegal_op | 1st: NOP -> illegal instruction exception
      0x00008067  // ret (32-bit)
    };

    tmp_a = (uint32_t)&dummy_sub_program_ci; // call the dummy sub program
    asm volatile ("jalr ra, %[input_i]" :  : [input_i] "r" (tmp_a));

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_I_ILLEGAL) {
      test_ok();
    }
    else {
      test_fail();
    }
  }
  else {
    PRINT_STANDARD("skipped (n.a. with C-ext)\n");
  }


  // ----------------------------------------------------------
  // Breakpoint instruction
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] BREAK (break instr.) EXC: ", cnt_test);
  cnt_test++;

  asm volatile("EBREAK");

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_BREAKPOINT) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Unaligned load address
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] L_ALIGN (load addr alignment) EXC: ", cnt_test);
  cnt_test++;

  // load from unaligned address
  neorv32_cpu_load_unsigned_word(ADDR_UNALIGNED);

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_L_MISALIGNED) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Load access fault
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] L_ACC (load bus access) EXC: ", cnt_test);
  cnt_test++;

  // load from unreachable aligned address
  neorv32_cpu_load_unsigned_word(ADDR_UNREACHABLE);

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_L_ACCESS) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Unaligned store address
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] S_ALIGN (store addr alignment) EXC: ", cnt_test);
  cnt_test++;

  // store to unaligned address
  asm volatile ("sw zero, %[input_i](zero)" :  : [input_i] "i" (ADDR_UNALIGNED));

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_S_MISALIGNED) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Store access fault
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] S_ACC (store bus access) EXC: ", cnt_test);
  cnt_test++;

  // store to unreachable aligned address
  neorv32_cpu_store_unsigned_word(ADDR_UNREACHABLE, 0);

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_S_ACCESS) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Environment call from M-mode
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] ENVCALL (ecall instr.) from M-mode EXC: ", cnt_test);
  cnt_test++;

  asm volatile("ECALL");

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_MENV_CALL) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Environment call from U-mode
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] ENVCALL (ecall instr.) from U-mode EXC: ", cnt_test);

  // skip if U-mode is not implemented
  if (neorv32_cpu_csr_read(CSR_MISA) & (1<<CSR_MISA_U)) {

    cnt_test++;

    // switch to user mode (hart will be back in MACHINE mode when trap handler returns)
    neorv32_cpu_goto_user_mode();
    {
      asm volatile("ECALL");
    }

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_UENV_CALL) {
      test_ok();
    }
    else {
      test_fail();
    }

  }
  else {
    PRINT_STANDARD("skipped (n.a. without U-ext)\n");
  }


  // ----------------------------------------------------------
  // Machine timer interrupt (MTIME)
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] MTI (via MTIME): ", cnt_test);

  cnt_test++;

  // configure MTIME IRQ (and check overflow form low owrd to high word)
  neorv32_mtime_set_timecmp(-1);
  neorv32_mtime_set_time(0);

  neorv32_cpu_csr_write(CSR_MIP, 0); // clear all pending IRQs

  neorv32_mtime_set_timecmp(0x0000000100000000ULL);
  neorv32_mtime_set_time(   0x00000000FFFFFFFEULL);

  // wait some time for the IRQ to trigger and arrive the CPU
  asm volatile("nop");
  asm volatile("nop");
  asm volatile("nop");
  asm volatile("nop");

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_MTI) {
    test_ok();
  }
  else {
    test_fail();
  }

  // no more mtime interrupts
  neorv32_mtime_set_timecmp(-1);


  // ----------------------------------------------------------
  // Machine software interrupt (MSI) via testbench
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] MSI (via testbench): ", cnt_test);

  cnt_test++;

  // trigger IRQ
  sim_irq_trigger(1 << CSR_MIE_MSIE);

  // wait some time for the IRQ to arrive the CPU
  asm volatile("nop");
  asm volatile("nop");

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_MSI) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Machine external interrupt (MEI) via testbench
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] MEI (via testbench): ", cnt_test);

  cnt_test++;

  // trigger IRQ
  sim_irq_trigger(1 << CSR_MIE_MEIE);

  // wait some time for the IRQ to arrive the CPU
  asm volatile("nop");
  asm volatile("nop");

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_MEI) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Non-maskable interrupt (NMI) via testbench
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] NMI (via testbench): ", cnt_test);

  cnt_test++;

  // trigger IRQ
  sim_irq_trigger(1 << 0);

  // wait some time for the IRQ to arrive the CPU
  asm volatile("nop");
  asm volatile("nop");

  if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_NMI) {
    test_ok();
  }
  else {
    test_fail();
  }


  // ----------------------------------------------------------
  // Fast interrupt channel 0 (WDT)
  // ----------------------------------------------------------
  if (neorv32_wdt_available()) {
    neorv32_cpu_csr_write(CSR_MCAUSE, 0);
    PRINT_STANDARD("[%i] FIRQ0 test (via WDT): ", cnt_test);

    cnt_test++;

    // enable fast interrupt
    neorv32_cpu_irq_enable(CSR_MIE_FIRQ0E);

    // configure WDT
    neorv32_wdt_setup(CLK_PRSC_4096, 0, 1); // highest clock prescaler, trigger IRQ on timeout, lock access
    WDT_CT = 0; // try to deactivate WDT (should fail as access is loced)
    neorv32_wdt_force(); // force watchdog into action

    // wait some time for the IRQ to arrive the CPU
    asm volatile("nop");
    asm volatile("nop");

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_FIRQ_0) {
      test_ok();
    }
    else {
      test_fail();
    }

    // no more WDT interrupts
    neorv32_wdt_disable();

    // disable fast interrupt
    neorv32_cpu_irq_disable(CSR_MIE_FIRQ0E);
  }


  // ----------------------------------------------------------
  // Fast interrupt channel 1 (CFS)
  // ----------------------------------------------------------
  PRINT_STANDARD("[%i] FIRQ1 test (via CFS): ", cnt_test);
  PRINT_STANDARD("skipped (n.a.)\n");


  // ----------------------------------------------------------
  // Fast interrupt channel 2 (UART0.RX)
  // ----------------------------------------------------------
  if (neorv32_uart1_available()) {
    neorv32_cpu_csr_write(CSR_MCAUSE, 0);
    PRINT_STANDARD("[%i] FIRQ2 test (via UART0.RX): ", cnt_test);

    cnt_test++;

    // enable fast interrupt
    neorv32_cpu_irq_enable(CSR_MIE_FIRQ2E);

    // wait for UART0 to finish transmitting
    while(neorv32_uart0_tx_busy());

    // backup current UART0 configuration
    tmp_a = UART0_CT;

    // make sure UART is enabled
    UART0_CT |= (1 << UART_CT_EN);
    // make sure sim mode is disabled
    UART0_CT &= ~(1 << UART_CT_SIM_MODE);

    // trigger UART0 RX IRQ
    neorv32_uart0_putc(0);

    // wait for UART0 to finish transmitting
    while(neorv32_uart0_tx_busy());

    // wait some time for the IRQ to arrive the CPU
    asm volatile("nop");
    asm volatile("nop");

    // restore original configuration
    UART0_CT = tmp_a;

    // disable fast interrupt
    neorv32_cpu_irq_disable(CSR_MIE_FIRQ2E);

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_FIRQ_2) {
      test_ok();
    }
    else {
      test_fail();
    }
  }


  // ----------------------------------------------------------
  // Fast interrupt channel 3 (UART0.TX)
  // ----------------------------------------------------------
  if (neorv32_uart0_available()) {
    neorv32_cpu_csr_write(CSR_MCAUSE, 0);
    PRINT_STANDARD("[%i] FIRQ3 test (via UART0.TX): ", cnt_test);

    cnt_test++;

    // UART0 TX interrupt enable
    neorv32_cpu_irq_enable(CSR_MIE_FIRQ3E);

    // wait for UART0 to finish transmitting
    while(neorv32_uart0_tx_busy());

    // backup current UART0 configuration
    tmp_a = UART0_CT;

    // make sure UART is enabled
    UART0_CT |= (1 << UART_CT_EN);
    // make sure sim mode is disabled
    UART0_CT &= ~(1 << UART_CT_SIM_MODE);

    // trigger UART0 TX IRQ
    neorv32_uart0_putc(0);

    // wait for UART to finish transmitting
    while(neorv32_uart0_tx_busy());

    // wait some time for the IRQ to arrive the CPU
    asm volatile("nop");
    asm volatile("nop");

    // restore original configuration
    UART0_CT = tmp_a;

    neorv32_cpu_irq_disable(CSR_MIE_FIRQ3E);

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_FIRQ_3) {
      test_ok();
    }
    else {
      test_fail();
    }
  }


  // ----------------------------------------------------------
  // Fast interrupt channel 4 (UART1.RX)
  // ----------------------------------------------------------
  if (neorv32_uart1_available()) {
    neorv32_cpu_csr_write(CSR_MCAUSE, 0);
    PRINT_STANDARD("[%i] FIRQ4 test (via UART1.RX): ", cnt_test);

    cnt_test++;

    // UART1 RX interrupt enable
    neorv32_cpu_irq_enable(CSR_MIE_FIRQ4E);

    // backup current UART1 configuration
    tmp_a = UART1_CT;

    // make sure UART is enabled
    UART1_CT |= (1 << UART_CT_EN);
    // make sure sim mode is disabled
    UART1_CT &= ~(1 << UART_CT_SIM_MODE);

    // trigger UART1 RX IRQ
    neorv32_uart1_putc(0);

    // wait for UART1 to finish transmitting
    while(neorv32_uart1_tx_busy());

    // wait some time for the IRQ to arrive the CPU
    asm volatile("nop");
    asm volatile("nop");

    // restore original configuration
    UART1_CT = tmp_a;

    // disable fast interrupt
    neorv32_cpu_irq_disable(CSR_MIE_FIRQ4E);

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_FIRQ_4) {
      test_ok();
    }
    else {
      test_fail();
    }
  }


  // ----------------------------------------------------------
  // Fast interrupt channel 5 (UART1.TX)
  // ----------------------------------------------------------
  if (neorv32_uart1_available()) {
    neorv32_cpu_csr_write(CSR_MCAUSE, 0);
    PRINT_STANDARD("[%i] FIRQ5 test (via UART1.TX): ", cnt_test);

    cnt_test++;

    // UART1 RX interrupt enable
    neorv32_cpu_irq_enable(CSR_MIE_FIRQ5E);

    // backup current UART1 configuration
    tmp_a = UART1_CT;

    // make sure UART is enabled
    UART1_CT |= (1 << UART_CT_EN);
    // make sure sim mode is disabled
    UART1_CT &= ~(1 << UART_CT_SIM_MODE);

    // trigger UART1 TX IRQ
    neorv32_uart1_putc(0);

    // wait for UART1 to finish transmitting
    while(neorv32_uart1_tx_busy());

    // wait some time for the IRQ to arrive the CPU
    asm volatile("nop");
    asm volatile("nop");

    // restore original configuration
    UART1_CT = tmp_a;

    // disable fast interrupt
    neorv32_cpu_irq_disable(CSR_MIE_FIRQ5E);

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_FIRQ_5) {
      test_ok();
    }
    else {
      test_fail();
    }
  }


  // ----------------------------------------------------------
  // Fast interrupt channel 6 (SPI)
  // ----------------------------------------------------------
  if (neorv32_spi_available()) {
    neorv32_cpu_csr_write(CSR_MCAUSE, 0);
    PRINT_STANDARD("[%i] FIRQ6 test (via SPI): ", cnt_test);

    cnt_test++;

    // enable fast interrupt
    neorv32_cpu_irq_enable(CSR_MIE_FIRQ6E);

    // configure SPI
    neorv32_spi_setup(CLK_PRSC_2, 0, 0);

    // trigger SPI IRQ
    neorv32_spi_trans(0);
    while(neorv32_spi_busy()); // wait for current transfer to finish

    // wait some time for the IRQ to arrive the CPU
    asm volatile("nop");
    asm volatile("nop");

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_FIRQ_6) {
      test_ok();
    }
    else {
      test_fail();
    }

    // disable SPI
    neorv32_spi_disable();

    // disable fast interrupt
    neorv32_cpu_irq_disable(CSR_MIE_FIRQ6E);
  }


  // ----------------------------------------------------------
  // Fast interrupt channel 7 (TWI)
  // ----------------------------------------------------------
  if (neorv32_twi_available()) {
    neorv32_cpu_csr_write(CSR_MCAUSE, 0);
    PRINT_STANDARD("[%i] FIRQ7 test (via TWI): ", cnt_test);

    cnt_test++;

    // configure TWI, fastest clock, no peripheral clock stretching
    neorv32_twi_setup(CLK_PRSC_2, 0);

    neorv32_cpu_irq_enable(CSR_MIE_FIRQ7E);

    // trigger TWI IRQ
    neorv32_twi_generate_start();
    neorv32_twi_trans(0);
    neorv32_twi_generate_stop();

    // wait some time for the IRQ to arrive the CPU
    asm volatile("nop");
    asm volatile("nop");

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_FIRQ_7) {
      test_ok();
    }
    else {
      test_fail();
    }

    // disable TWI
    neorv32_twi_disable();
    neorv32_cpu_irq_disable(CSR_MIE_FIRQ7E);
  }


  // ----------------------------------------------------------
  // Fast interrupt channel 8 (XIRQ)
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] FIRQ8 test (via XIRQ): ", cnt_test);
  if (neorv32_xirq_available()) {

    cnt_test++;

    int xirq_err_cnt = 0;
    xirq_trap_handler_ack = 0;

    xirq_err_cnt += neorv32_xirq_setup(); // initialize XIRQ
    xirq_err_cnt += neorv32_xirq_install(0, xirq_trap_handler0); // install XIRQ IRQ handler channel 0
    xirq_err_cnt += neorv32_xirq_install(1, xirq_trap_handler1); // install XIRQ IRQ handler channel 1

    neorv32_xirq_global_enable(); // enable XIRQ FIRQ

    // trigger XIRQ channel 1 and 0
    neorv32_gpio_port_set(3);

    // wait for IRQs to arrive CPU
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");

    if ((neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_FIRQ_8) && // FIRQ8 IRQ
        (xirq_err_cnt == 0) && // no errors during XIRQ configuration
        (xirq_trap_handler_ack == 4)) { // XIRQ channel handler 0 executed before handler 1
      test_ok();
    }
    else {
      test_fail();
    }

    neorv32_xirq_global_disable();
    XIRQ_IER = 0;
    XIRQ_IPR = -1;
  }


  // ----------------------------------------------------------
  // Fast interrupt channel 9 (NEOLED)
  // ----------------------------------------------------------
  PRINT_STANDARD("[%i] FIRQ9 (NEOLED): skipped\n", cnt_test);


  // ----------------------------------------------------------
  // Fast interrupt channel 10 & 11 (SLINK)
  // ----------------------------------------------------------
  if (neorv32_slink_available()) {
    neorv32_cpu_csr_write(CSR_MCAUSE, 0);
    PRINT_STANDARD("[%i] FIRQ10 & 11 (SLINK): ", cnt_test);

    cnt_test++;

    // enable SLINK
    neorv32_slink_enable();

    // enable SLINK FIRQs
    neorv32_cpu_irq_enable(CSR_MIE_FIRQ10E);
    neorv32_cpu_irq_enable(CSR_MIE_FIRQ11E);

    tmp_a = 0; // error counter

    // send single data word via link 0
    if (neorv32_slink_tx0_nonblocking(0xA1B2C3D4)) {
      tmp_a++; // sending failed
    }

    // get single data word from link 0
    uint32_t slink_rx_data;
    if (neorv32_slink_rx0_nonblocking(&slink_rx_data)) {
      tmp_a++; // receiving failed
    }

    // wait some time for the IRQ to arrive the CPU
    asm volatile("nop");
    asm volatile("nop");

    tmp_b = neorv32_cpu_csr_read(CSR_MCAUSE);
    if (((tmp_b == TRAP_CODE_FIRQ_10) || (tmp_b == TRAP_CODE_FIRQ_11)) && // right trap code
        (tmp_a == 0) && // local error counter = 0
        (slink_rx_data == 0xA1B2C3D4)) { // correct data read-back
      test_ok();
    }
    else {
      test_fail();
    }

    // shutdown SLINK
    neorv32_slink_disable();
  }


//// ----------------------------------------------------------
//// Fast interrupt channel 12..15 (reserved)
//// ----------------------------------------------------------
//PRINT_STANDARD("[%i] FIRQ12..15: ", cnt_test);
//PRINT_STANDARD("skipped (n.a.)\n");


  // ----------------------------------------------------------
  // Test WFI ("sleep") instructions, wakeup via MTIME
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] WFI (sleep instruction) test (wake-up via MTIME): ", cnt_test);

  cnt_test++;

  // program wake-up timer
  neorv32_mtime_set_timecmp(neorv32_mtime_get_time() + 1000);

  // clear timeout wait flag
  tmp_a = neorv32_cpu_csr_read(CSR_MSTATUS);
  tmp_a &= ~(1 << CSR_MSTATUS_TW);
  neorv32_cpu_csr_write(CSR_MSTATUS, tmp_a);

  // switch to user mode (hart will be back in MACHINE mode when trap handler returns)
  neorv32_cpu_goto_user_mode();
  {
    // only when mstatus.TW = 0 executing WFI in user mode is allowed
    asm volatile ("wfi"); // put CPU into sleep mode
  }

  if (neorv32_cpu_csr_read(CSR_MCAUSE) != TRAP_CODE_MTI) {
    test_fail();
  }
  else {
    test_ok();
  }

  // no more mtime interrupts
  neorv32_mtime_set_timecmp(-1);


  // ----------------------------------------------------------
  // Test invalid CSR access in user mode
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] Invalid CSR access (mstatus) from user mode: ", cnt_test);

  // skip if U-mode is not implemented
  if (neorv32_cpu_csr_read(CSR_MISA) & (1<<CSR_MISA_U)) {

    cnt_test++;

    // switch to user mode (hart will be back in MACHINE mode when trap handler returns)
    neorv32_cpu_goto_user_mode();
    {
      // access to misa not allowed for user-level programs
      tmp_a = neorv32_cpu_csr_read(CSR_MISA);
    }

    if (tmp_a != 0) {
      PRINT_CRITICAL("%c[1m<SECURITY FAILURE> %c[0m\n", 27, 27);
    }

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_I_ILLEGAL) {
      test_ok();
    }
    else {
      test_fail();
    }

  }
  else {
    PRINT_STANDARD("skipped (n.a. without U-ext)\n");
  }


  // ----------------------------------------------------------
  // Test RTE debug trap handler
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] RTE debug trap handler: ", cnt_test);

  cnt_test++;

  // uninstall custom handler and use default RTE debug handler
  neorv32_rte_exception_uninstall(RTE_TRAP_I_ILLEGAL);

  // trigger illegal instruction exception
  neorv32_cpu_csr_read(0xfff); // CSR not available

  PRINT_STANDARD(" ");
  if (neorv32_cpu_csr_read(CSR_MCAUSE) != 0) {
    test_ok();
  }
  else {
    test_fail();
    PRINT_STANDARD("answer: 0x%x", neorv32_cpu_csr_read(CSR_MCAUSE));
  }

  // restore original handler
  neorv32_rte_exception_install(RTE_TRAP_I_ILLEGAL, global_trap_handler);


  // ----------------------------------------------------------
  // Test physical memory protection
  // ----------------------------------------------------------
  PRINT_STANDARD("[%i] PMP - Physical memory protection: ", cnt_test);

  // check if PMP is implemented
  if (neorv32_cpu_pmp_get_num_regions() != 0)  {

    // Create PMP protected region
    // ---------------------------------------------
    neorv32_cpu_csr_write(CSR_MCAUSE, 0);
    cnt_test++;

    // find out minimal region size (granularity)
    tmp_b = neorv32_cpu_pmp_get_granularity();

    tmp_a = SYSINFO_DSPACE_BASE; // base address of protected region
    PRINT_STANDARD("Creating protected page (NAPOT, [!X,!W,!R], %u bytes) @ 0x%x: ", tmp_b, tmp_a);

    // configure
    int pmp_return = neorv32_cpu_pmp_configure_region(0, tmp_a, tmp_b, 0b00011000); // NAPOT, NO read permission, NO write permission, and NO execute permissions

    if ((pmp_return == 0) && (neorv32_cpu_csr_read(CSR_MCAUSE) == 0)) {
      test_ok();
    }
    else {
      test_fail();
    }


    // ------ EXECUTE: should fail ------
    PRINT_STANDARD("[%i] PMP: U-mode execute: ", cnt_test);
    cnt_test++;
    neorv32_cpu_csr_write(CSR_MCAUSE, 0);

    // switch to user mode (hart will be back in MACHINE mode when trap handler returns)
    neorv32_cpu_goto_user_mode();
    {
      asm volatile ("jalr ra, %[input_i]" :  : [input_i] "r" (tmp_a)); // call address to execute -> should fail
    }

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == 0) {
      // switch back to machine mode (if not already)
      asm volatile ("ecall");

      test_fail();
    }
    else {
      // switch back to machine mode (if not already)
      asm volatile ("ecall");

      test_ok();
    }


    // ------ LOAD: should fail ------
    PRINT_STANDARD("[%i] PMP: U-mode read: ", cnt_test);
    cnt_test++;
    neorv32_cpu_csr_write(CSR_MCAUSE, 0);

    // switch to user mode (hart will be back in MACHINE mode when trap handler returns)
    neorv32_cpu_goto_user_mode();
    {
      tmp_b = neorv32_cpu_load_unsigned_word(tmp_a); // load access -> should fail
    }

    if (tmp_b != 0) {
      PRINT_CRITICAL("%c[1m<SECURITY FAILURE> %c[0m\n", 27, 27);
    }

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_L_ACCESS) {
      // switch back to machine mode (if not already)
      asm volatile ("ecall");

      test_ok();
    }
    else {
      // switch back to machine mode (if not already)
      asm volatile ("ecall");

      test_fail();
    }


    // ------ STORE: should fail ------
    PRINT_STANDARD("[%i] PMP: U-mode write: ", cnt_test);
    cnt_test++;
    neorv32_cpu_csr_write(CSR_MCAUSE, 0);

    // switch to user mode (hart will be back in MACHINE mode when trap handler returns)
    neorv32_cpu_goto_user_mode();
    {
      neorv32_cpu_store_unsigned_word(tmp_a, 0); // store access -> should fail
    }

    if (neorv32_cpu_csr_read(CSR_MCAUSE) == TRAP_CODE_S_ACCESS) {
      // switch back to machine mode (if not already)
      asm volatile ("ecall");

      test_ok();
    }
    else {
      // switch back to machine mode (if not already)
      asm volatile ("ecall");

      test_fail();
    }


    // ------ Lock test - pmpcfg0.0 / pmpaddr0 ------
    PRINT_STANDARD("[%i] PMP: Entry [mode=off] lock: ", cnt_test);
    cnt_test++;
    neorv32_cpu_csr_write(CSR_MCAUSE, 0);

    neorv32_cpu_csr_write(CSR_PMPCFG0, 0b10000001); // locked, but entry is deactivated (mode = off)

    // make sure a locked cfg cannot be written
    tmp_a = neorv32_cpu_csr_read(CSR_PMPCFG0);
    neorv32_cpu_csr_write(CSR_PMPCFG0, 0b00011001); // try to re-write CFG content

    tmp_b = neorv32_cpu_csr_read(CSR_PMPADDR0);
    neorv32_cpu_csr_write(CSR_PMPADDR0, 0xABABCDCD); // try to re-write ADDR content

    if ((tmp_a != neorv32_cpu_csr_read(CSR_PMPCFG0)) || (tmp_b != neorv32_cpu_csr_read(CSR_PMPADDR0)) || (neorv32_cpu_csr_read(CSR_MCAUSE) != 0)) {
      test_fail();
    }
    else {
      test_ok();
    }

  }
  else {
    PRINT_STANDARD("skipped (n.a.)\n");
  }


  // ----------------------------------------------------------
  // Test atomic LR/SC operation - should succeed
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] Atomic access (LR+SC succeeding access): ", cnt_test);

#ifdef __riscv_atomic
  // skip if A-mode is not implemented
  if ((neorv32_cpu_csr_read(CSR_MISA) & (1<<CSR_MISA_A)) != 0) {

    cnt_test++;

    neorv32_cpu_store_unsigned_word((uint32_t)&atomic_access_addr, 0x11223344);

    tmp_a = neorv32_cpu_load_reservate_word((uint32_t)&atomic_access_addr); // make reservation
    asm volatile ("nop");
    tmp_b = neorv32_cpu_store_conditional((uint32_t)&atomic_access_addr, 0x22446688);

    // atomic access
    if ((tmp_b == 0) && // status: success
        (tmp_a == 0x11223344) && // correct data read
        (neorv32_cpu_load_unsigned_word((uint32_t)&atomic_access_addr) == 0x22446688) && // correct data write
        (neorv32_cpu_csr_read(CSR_MCAUSE) == 0)) { // no exception triggered
      test_ok();
    }
    else {
      test_fail();
    }
  }
  else {
    PRINT_STANDARD("skipped (n.a.)\n");
  }
#else
  PRINT_STANDARD("skipped (n.a.)\n");
#endif


  // ----------------------------------------------------------
  // Test atomic LR/SC operation - should fail
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] Atomic access (LR+SC failing access 1): ", cnt_test);

#ifdef __riscv_atomic
  // skip if A-mode is not implemented
  if ((neorv32_cpu_csr_read(CSR_MISA) & (1<<CSR_MISA_A)) != 0) {

    cnt_test++;

    neorv32_cpu_store_unsigned_word((uint32_t)&atomic_access_addr, 0xAABBCCDD);

    // atomic access
    tmp_a = neorv32_cpu_load_reservate_word((uint32_t)&atomic_access_addr); // make reservation
    neorv32_cpu_store_unsigned_word((uint32_t)&atomic_access_addr, 0xDEADDEAD); // destroy reservation
    tmp_b = neorv32_cpu_store_conditional((uint32_t)&atomic_access_addr, 0x22446688);

    if ((tmp_b != 0) && // status: fail
        (tmp_a == 0xAABBCCDD) && // correct data read
        (neorv32_cpu_load_unsigned_word((uint32_t)&atomic_access_addr) == 0xDEADDEAD)) { // correct data write
      test_ok();
    }
    else {
      test_fail();
    }
  }
  else {
    PRINT_STANDARD("skipped (n.a.)\n");
  }
#else
  PRINT_STANDARD("skipped (n.a.)\n");
#endif


  // ----------------------------------------------------------
  // Test atomic LR/SC operation - should fail
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCAUSE, 0);
  PRINT_STANDARD("[%i] Atomic access (LR+SC failing access 2): ", cnt_test);

#ifdef __riscv_atomic
  // skip if A-mode is not implemented
  if ((neorv32_cpu_csr_read(CSR_MISA) & (1<<CSR_MISA_A)) != 0) {

    cnt_test++;

    neorv32_cpu_store_unsigned_word((uint32_t)&atomic_access_addr, 0x12341234);

    // atomic access
    tmp_a = neorv32_cpu_load_reservate_word((uint32_t)&atomic_access_addr); // make reservation
    asm volatile ("ecall"); // destroy reservation via trap (simulate a context switch)
    tmp_b = neorv32_cpu_store_conditional((uint32_t)&atomic_access_addr, 0xDEADBEEF);

    if ((tmp_b != 0) && // status: fail
        (tmp_a == 0x12341234) && // correct data read
        (neorv32_cpu_load_unsigned_word((uint32_t)&atomic_access_addr) == 0x12341234)) { // correct data write
      test_ok();
    }
    else {
      test_fail();
    }
  }
  else {
    PRINT_STANDARD("skipped (on real HW)\n");
  }
#else
  PRINT_STANDARD("skipped (n.a.)\n");
#endif


  // ----------------------------------------------------------
  // HPM reports
  // ----------------------------------------------------------
  neorv32_cpu_csr_write(CSR_MCOUNTINHIBIT, -1); // stop all counters
  PRINT_STANDARD("\n\n-- HPM reports LOW (%u HPMs available) --\n", num_hpm_cnts_global);
  PRINT_STANDARD("#IR - Instr.:   %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_INSTRET)); // = "HPM_0"
//PRINT_STANDARD("#TM - Time:     %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_TIME));    // = "HPM_1"
  PRINT_STANDARD("#CY - CLKs:     %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_CYCLE));   // = "HPM_2"
  PRINT_STANDARD("#03 - Compr.:   %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_MHPMCOUNTER3));
  PRINT_STANDARD("#04 - IF wait:  %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_MHPMCOUNTER4));
  PRINT_STANDARD("#05 - II wait:  %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_MHPMCOUNTER5));
  PRINT_STANDARD("#06 - ALU wait: %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_MHPMCOUNTER6));
  PRINT_STANDARD("#07 - Loads:    %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_MHPMCOUNTER7));
  PRINT_STANDARD("#08 - Stores:   %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_MHPMCOUNTER8));
  PRINT_STANDARD("#09 - MEM wait: %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_MHPMCOUNTER9));
  PRINT_STANDARD("#10 - Jumps:    %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_MHPMCOUNTER10));
  PRINT_STANDARD("#11 - Branches: %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_MHPMCOUNTER11));
  PRINT_STANDARD("#12 -  Taken:   %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_MHPMCOUNTER12));
  PRINT_STANDARD("#13 - Traps:    %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_MHPMCOUNTER13));
  PRINT_STANDARD("#14 - Illegals: %u\n", (uint32_t)neorv32_cpu_csr_read(CSR_MHPMCOUNTER14));


  // ----------------------------------------------------------
  // Final test reports
  // ----------------------------------------------------------
  PRINT_CRITICAL("\n\nTest results:\nPASS: %i/%i\nFAIL: %i/%i\n\n", cnt_ok, cnt_test, cnt_fail, cnt_test);

  // final result
  if (cnt_fail == 0) {
    PRINT_STANDARD("%c[1m[CPU TEST COMPLETED SUCCESSFULLY!]%c[0m\n", 27, 27);
  }
  else {
    PRINT_STANDARD("%c[1m[CPU TEST FAILED!]%c[0m\n", 27, 27);
  }

  return (int)cnt_fail; // return error counter for after-main handler
}


/**********************************************************************//**
 * Simulation-based function to trigger CPU interrupts (MSI, MEI, FIRQ4..7).
 *
 * @param[in] sel IRQ select mask (bit positions according to #NEORV32_CSR_MIE_enum).
 **************************************************************************/
void sim_irq_trigger(uint32_t sel) {

  *(IO_REG32 (0xFF000000)) = sel;
}


/**********************************************************************//**
 * Trap handler for ALL exceptions/interrupts.
 **************************************************************************/
void global_trap_handler(void) {

  // hack: always come back in MACHINE MODE
  register uint32_t mask = (1<<CSR_MSTATUS_MPP_H) | (1<<CSR_MSTATUS_MPP_L);
  asm volatile ("csrrs zero, mstatus, %[input_j]" :  : [input_j] "r" (mask));
}


/**********************************************************************//**
 * XIRQ handler channel 0.
 **************************************************************************/
void xirq_trap_handler0(void) {

  xirq_trap_handler_ack += 2;
}


/**********************************************************************//**
 * XIRQ handler channel 1.
 **************************************************************************/
void xirq_trap_handler1(void) {

  xirq_trap_handler_ack *= 2;
}


/**********************************************************************//**
 * Test results helper function: Shows "[ok]" and increments global cnt_ok
 **************************************************************************/
void test_ok(void) {

  PRINT_STANDARD("%c[1m[ok]%c[0m\n", 27, 27);
  cnt_ok++;
}


/**********************************************************************//**
 * Test results helper function: Shows "[FAIL]" and increments global cnt_fail
 **************************************************************************/
void test_fail(void) {

  PRINT_CRITICAL("%c[1m[FAIL]%c[0m\n", 27, 27);
  cnt_fail++;
}


/**********************************************************************//**
 * "after-main" handler that is executed after the application's
 * main function returns (called by crt0.S start-up code): Output minimal
 * test report to physical UART
 **************************************************************************/
int __neorv32_crt0_after_main(int32_t return_code) {

  // make sure sim mode is disabled and UARTs are actually enabled
  UART0_CT |=  (1 << UART_CT_EN);
  UART0_CT &= ~(1 << UART_CT_SIM_MODE);
  UART1_CT = UART0_CT;

  // minimal result report
  PRINT_CRITICAL("%u/%u\n", (uint32_t)return_code, (uint32_t)cnt_test);

  return 0;
}
