#include "splatterhouse/hle_kernel.h"
#include "splatterhouse/memory.h"
#include "splatterhouse/common.h"
#include <spdlog/spdlog.h>

namespace splatterhouse::hle {

// ── xboxkrnl.exe (kernel) stubs ───────────────────────────────────────
// Referencia: xenia/kernel/xboxkrnl/

static void Krnl_KeSetBaseFileTimestamp() { UnimplementedStub("xboxkrnl", "KeSetBaseFileTimestamp"); }
static void Krnl_ExCreateThread() { UnimplementedStub("xboxkrnl", "ExCreateThread"); }
static void Krnl_KeDelayExecution() { UnimplementedStub("xboxkrnl", "KeDelayExecution"); }
static void Krnl_MmAllocatePhysicalMemory() { UnimplementedStub("xboxkrnl", "MmAllocatePhysicalMemory"); }
static void Krnl_NtAllocateVirtualMemory() { UnimplementedStub("xboxkrnl", "NtAllocateVirtualMemory"); }
static void Krnl_ObCreateSymbolicLink() { UnimplementedStub("xboxkrnl", "ObCreateSymbolicLink"); }
static void Krnl_RtlInitAnsiString() { UnimplementedStub("xboxkrnl", "RtlInitAnsiString"); }

void RegisterXboxkrnlExports() {
    auto reg = [](const char* name, uint32_t ord, HleHandler h){ Register("xboxkrnl.exe", name, ord, h); };
    reg("KeSetBaseFileTimestamp", 0, Krnl_KeSetBaseFileTimestamp);
    reg("ExCreateThread", 0, Krnl_ExCreateThread);
    reg("KeDelayExecution", 0, Krnl_KeDelayExecution);
    reg("MmAllocatePhysicalMemory", 0, Krnl_MmAllocatePhysicalMemory);
    reg("NtAllocateVirtualMemory", 0, Krnl_NtAllocateVirtualMemory);
    reg("ObCreateSymbolicLink", 0, Krnl_ObCreateSymbolicLink);
    reg("RtlInitAnsiString", 0, Krnl_RtlInitAnsiString);
    spdlog::debug("[HLE] xboxkrnl.exe exports registrados");
}

} // namespace splatterhouse::hle

extern "C" {
    void __imp__xboxkrnl_KeSetBaseFileTimestamp() { splatterhouse::hle::UnimplementedStub("xboxkrnl","KeSetBaseFileTimestamp"); }
    void __imp__xboxkrnl_ExCreateThread() { splatterhouse::hle::UnimplementedStub("xboxkrnl","ExCreateThread"); }
    void __imp__xboxkrnl_MmAllocatePhysicalMemory() { splatterhouse::hle::UnimplementedStub("xboxkrnl","MmAllocatePhysicalMemory"); }
}
