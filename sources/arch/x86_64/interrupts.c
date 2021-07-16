#include <brutal/log.h>
#include <brutal/sync.h>
#include "arch/cpu.h"
#include "arch/x86_64//syscall.h"
#include "arch/x86_64/apic.h"
#include "arch/x86_64/asm.h"
#include "arch/x86_64/interrupts.h"
#include "arch/x86_64/pic.h"
#include "arch/x86_64/simd.h"
#include "arch/x86_64/smp.h"
#include "arch/x86_64/tasking.h"
#include "brutal/types.h"
#include "kernel/tasking.h"

Lock error_lock;

static char *_exception_messages[32] = {
    "DivisionByZero",
    "Debug",
    "NonMaskableInterrupt",
    "Breakpoint",
    "DetectedOverflow",
    "OutOfBounds",
    "InvalidOpcode",
    "NoCoprocessor",
    "DoubleFault",
    "CoprocessorSegmentOverrun",
    "BadTss",
    "SegmentNotPresent",
    "StackFault",
    "GeneralProtectionFault",
    "PageFault",
    "UnknownInterrupt",
    "CoprocessorFault",
    "AlignmentCheck",
    "MachineCheck",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
};

void dump_register(Regs const *regs)
{
    log_unlock("RIP: {#016p} | RSP: {#016p}", regs->rip, regs->rsp);
    log_unlock("CR2: {#016p} | CR3: {#016p} ", asm_read_cr2(), asm_read_cr3());
    log_unlock("CS : {#02p} | SS : {#02p} | RFlags: {#p}", regs->cs, regs->ss, regs->rflags);

    log_unlock("");

    log_unlock("RAX: {#016p} | RBX: {#016p}", regs->rax, regs->rbx);
    log_unlock("RCX: {#016p} | RDX: {#016p}", regs->rcx, regs->rdx);
    log_unlock("RSI: {#016p} | RDI: {#016p}", regs->rsi, regs->rdi);
    log_unlock("RBP: {#016p} | R8 : {#016p}", regs->rbp, regs->r8);
    log_unlock("R9 : {#016p} | R10: {#016p}", regs->r9, regs->r10);
    log_unlock("R11: {#016p} | R12: {#016p}", regs->r11, regs->r12);
    log_unlock("R13: {#016p} | R14: {#016p}", regs->r13, regs->r14);
    log_unlock("R15: {#016p}", regs->r15);
}

struct stackframe
{
    struct stackframe *rbp;
    uint64_t rip;
};

void backtrace(uintptr_t rbp, uint64_t rip)
{
    auto stackframe = (struct stackframe *)(rbp);

    log("Backtrace:");
    log("{016x}", rip);
    while (stackframe)
    {
        log("{016x}", stackframe->rip);
        stackframe = stackframe->rbp;
    }
}

void interrupt_error_handler(Regs *regs, uintptr_t rsp)
{
    lock_acquire(&error_lock);

    smp_stop_all();

    log_unlock("");
    log_unlock("------------------------------------------------------------");
    log_unlock("");
    log_unlock("KERNEL PANIC ON CPU N°{}", cpu_self_id(), regs->rip, regs->rbp, rsp);
    log_unlock("");
    log_unlock("{}({}) with error_code={}!", _exception_messages[regs->int_no], regs->int_no, regs->error_code);
    log_unlock("");
    dump_register(regs);
    log_unlock("");
    backtrace(regs->rbp, regs->rip);
    log_unlock("");
    log_unlock("------------------------------------------------------------");
    log_unlock("");

    while (true)
    {
        asm_cli();
        asm_hlt();
    }
}

void scheduler_save_context(Regs const *regs)
{
    TaskImpl *impl = (TaskImpl *)task_self();

    simd_context_save(impl->simd_context);
    impl->regs = *regs;
}

void scheduler_load_context(Regs *regs)
{
    auto task = task_self();
    auto impl = (TaskImpl *)task_self();

    *regs = impl->regs;
    simd_context_load(impl->simd_context);

    memory_space_switch(task->space);
    syscall_set_stack(range_end(task->kernel_stack));
}

uint64_t interrupt_handler(uint64_t rsp)
{
    auto regs = (Regs *)rsp;

    if (regs->int_no < 32)
    {
        interrupt_error_handler(regs, rsp);
    }
    else if (regs->int_no == 32)
    {
        scheduler_save_context(regs);
        scheduler_schedule_and_switch();
        scheduler_load_context(regs);
    }
    else if (regs->int_no == IPIT_RESCHED)
    {
        scheduler_save_context(regs);
        scheduler_switch();
        scheduler_load_context(regs);
    }
    else if (regs->int_no == IPIT_STOP)
    {
        while (true)
        {
            asm_cli();
            asm_hlt();
        }
    }
    else if (regs->int_no == 0xf0)
    {
        log_unlock("Non maskable interrupt from APIC (Possible hardware error).");
    }

    apic_eoi();

    return rsp;
}
