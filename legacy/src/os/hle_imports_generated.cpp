#include "splatterhouse/common.h"
#include "ppc_context.h"
#include "splatterhouse/memory.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <atomic>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace splatterhouse;

// Shims de builtins que faltan en MSVC STL
extern "C" float roundevenf(float x) { return (float)std::nearbyint((double)x); }
extern "C" double roundeven(double x) { return std::nearbyint(x); }

// Tracking del ultimo import llamado (para diagnostico de crashes via VEH)
static std::atomic<const char*> g_lastImport{nullptr};
extern "C" const char* HLE_GetLastImport() { return g_lastImport.load(std::memory_order_relaxed); }
#define SH_TRACK() g_lastImport.store(__FUNCTION__, std::memory_order_relaxed)

// Base guest compartida para HLE que lanzan threads
static uint8_t* g_guestBase = nullptr;
extern "C" void HLE_SetGuestBase(uint8_t* b) { g_guestBase = b; }

// TLS por thread (r13 = thread pointer). La seccion .tls del XEX da los bytes iniciales.
static uint32_t s_tlsDataAddr = 0;
static uint32_t s_tlsDataSize = 0;
extern "C" void HLE_SetTlsData(uint32_t guestAddr, uint32_t size) {
    s_tlsDataAddr = guestAddr;
    s_tlsDataSize = size;
    spdlog::info("[HLE] TLS data en 0x{:08X} ({} bytes)", guestAddr, size);
}
// Devuelve el valor que debe tener r13 para el nuevo thread
extern "C" uint32_t HLE_AllocTlsThreadPointer() {
    constexpr uint32_t kTlsBlockSize = 0x20000;   // 128KB
    constexpr uint32_t kTlsPtrOffset = 0x10000;   // r13 a 64KB del inicio (offsets +/- ok)
    uint32_t block = mem::Alloc(kTlsBlockSize, 0x10000);
    if (block == 0) { spdlog::error("[HLE] TLS: sin memoria"); return 0; }
    if (s_tlsDataAddr && s_tlsDataSize && s_tlsDataSize < kTlsPtrOffset) {
        memcpy(mem::Translate(block), mem::Translate(s_tlsDataAddr), s_tlsDataSize);
    }
    return block + kTlsPtrOffset;
}

// Utilidades
static uint64_t NowFileTime100ns() {
    using namespace std::chrono;
    return (uint64_t)duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count() / 100 + 116444736000000000ull;
}

// thread exit exception
struct GuestThreadExit {};


// ============ Memoria ============
PPC_FUNC(__imp__NtAllocateVirtualMemory) {
    SH_TRACK();
    uint32_t baseAddrPtr = ctx.r3.u32;
    uint32_t regionSizePtr = ctx.r4.u32;
    uint32_t allocationType = ctx.r5.u32;
    uint32_t protect = ctx.r6.u32;
    uint32_t regionSize = mem::ReadBE32(regionSizePtr);
    if (regionSize == 0) regionSize = 0x100000;
    regionSize = (regionSize + 0xFFF) & ~0xFFFu;
    uint32_t allocAddr = mem::Alloc(regionSize, 0x1000);
    if (allocAddr == 0) {
        spdlog::error("[HLE] NtAllocateVirtualMemory: fallo al asignar {} bytes", regionSize);
        ctx.r3.u64 = 0xC0000017;
        return;
    }
    mem::WriteBE32(baseAddrPtr, allocAddr);
    spdlog::info("[HLE] NtAllocateVirtualMemory: {} bytes en 0x{:08X}", regionSize, allocAddr);
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtFreeVirtualMemory) {
    SH_TRACK();
    // Bump allocator sin free real
    ctx.r3.u64 = 0;
}

// ============ Proceso / Critical sections ============
PPC_FUNC(__imp__KeGetCurrentProcessType) {
    SH_TRACK();
    ctx.r3.u64 = 1; // retail title
}

static constexpr uint32_t kCriticalSectionSize = 0x18;

PPC_FUNC(__imp__RtlInitializeCriticalSection) {
    SH_TRACK();
    uint32_t cs = ctx.r3.u32;
    if (auto* p = (uint32_t*)mem::Translate(cs)) {
        memset(p, 0, kCriticalSectionSize);
        p[5] = bswap32(1000);
    }
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlInitializeCriticalSectionAndSpinCount) {
    SH_TRACK();
    uint32_t cs = ctx.r3.u32;
    if (auto* p = (uint32_t*)mem::Translate(cs)) {
        memset(p, 0, kCriticalSectionSize);
        p[5] = bswap32(ctx.r4.u32);
    }
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlEnterCriticalSection) {
    SH_TRACK();
    ctx.r3.u64 = 0; // TODO: semantica real multi-thread
}

PPC_FUNC(__imp__RtlLeaveCriticalSection) {
    SH_TRACK();
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlTryEnterCriticalSection) {
    SH_TRACK();
    ctx.r3.u64 = 1;
}

PPC_FUNC(__imp__KeEnterCriticalRegion) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__KeLeaveCriticalRegion) { SH_TRACK(); ctx.r3.u64 = 0; }

// ============ Tiempo ============
PPC_FUNC(__imp__KeQuerySystemTime) {
    SH_TRACK();
    if (auto* p = (uint64_t*)mem::Translate(ctx.r3.u32)) *p = bswap64(NowFileTime100ns());
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__KeQueryPerformanceCounter) {
    SH_TRACK();
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    if (auto* p = (uint64_t*)mem::Translate(ctx.r3.u32)) *p = bswap64((uint64_t)c.QuadPart);
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__KeQueryPerformanceFrequency) {
    SH_TRACK();
    LARGE_INTEGER f; QueryPerformanceFrequency(&f);
    if (auto* p = (uint64_t*)mem::Translate(ctx.r3.u32)) *p = bswap64((uint64_t)f.QuadPart);
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__KeDelayExecutionThread) {
    SH_TRACK();
    // r3 mode, r4 alertable, r5 = LARGE_INTEGER* interval (100ns, negativo = relativo)
    int64_t interval = 0;
    if (auto* p = (uint64_t*)mem::Translate(ctx.r5.u32)) interval = (int64_t)bswap64(*p);
    int64_t usec = (interval < 0 ? -interval : interval) / 10; // negativo = relativo
    if (usec > 0 && usec < 1000000) std::this_thread::sleep_for(std::chrono::microseconds(usec));
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtYieldExecution) { SH_TRACK(); std::this_thread::yield(); ctx.r3.u64 = 0; }

// ============ TLS ============
static constexpr uint32_t kTlsSlots = 1024;
static uint32_t s_tls[kTlsSlots] = {};
static uint32_t s_tlsNext = 1;

PPC_FUNC(__imp__KeTlsAlloc) {
    SH_TRACK();
    uint32_t slot = 0;
    if (s_tlsNext < kTlsSlots) { slot = s_tlsNext++; s_tls[slot] = 0; }
    else { spdlog::error("[HLE] KeTlsAlloc: sin slots"); }
    ctx.r3.u64 = slot;
}

PPC_FUNC(__imp__KeTlsFree) { SH_TRACK(); s_tls[ctx.r3.u32 % kTlsSlots] = 0; ctx.r3.u64 = 1; }

PPC_FUNC(__imp__KeTlsGetValue) {
    SH_TRACK();
    uint32_t slot = ctx.r3.u32;
    ctx.r3.u64 = (slot < kTlsSlots) ? s_tls[slot] : 0;
}

PPC_FUNC(__imp__KeTlsSetValue) {
    SH_TRACK();
    uint32_t slot = ctx.r3.u32;
    if (slot < kTlsSlots) s_tls[slot] = ctx.r4.u32;
    ctx.r3.u64 = 1;
}

// ============ Threads ============
static std::atomic<uint32_t> s_nextHandle{1};

PPC_FUNC(__imp__ExCreateThread) {
    SH_TRACK();
    // (PHANDLE Handle r3, DWORD StackSize r4, PDWORD ThreadId r5, PVOID SystemFrame r6,
    //  PTHREAD_START_ROUTINE StartRoutine r7, PVOID StartContext r8, DWORD Flags r9)
    uint32_t handlePtr = ctx.r3.u32;
    uint32_t stackSize = ctx.r4.u32;
    uint32_t threadIdPtr = ctx.r5.u32;
    uint32_t startRoutine = ctx.r7.u32;
    uint32_t startContext = ctx.r8.u32;
    uint32_t handle = 0xF1000000 + s_nextHandle.fetch_add(1);
    if (threadIdPtr) mem::WriteBE32(threadIdPtr, handle);
    if (handlePtr) mem::WriteBE32(handlePtr, handle);
    uint32_t ss = stackSize ? stackSize : 0x100000;
    ss = (ss + 0xFFF) & ~0xFFFu;
    uint32_t stackBase = mem::Alloc(ss, 0x1000);
    uint32_t stackTop = (stackBase + ss - 0x40) & ~0x1Fu;
    uint8_t* basePtr = g_guestBase;
    spdlog::info("[HLE] ExCreateThread start=0x{:08X} ctx=0x{:08X} stack=0x{:08X}-0x{:08X} handle=0x{:08X}",
                 startRoutine, startContext, stackBase, stackTop, handle);
    if (!basePtr || startRoutine == 0) { ctx.r3.u64 = 0; return; }
    // Resolver la funcion host del entry (tabla de funciones en guest memory)
    auto* fn = *(PPCFunc**)(basePtr + PPC_IMAGE_BASE + PPC_IMAGE_SIZE +
                            (uint64_t(uint32_t(startRoutine) - PPC_CODE_BASE) * 2));
    if (!fn) { spdlog::error("[HLE] ExCreateThread: entry 0x{:08X} no resuelto", startRoutine); ctx.r3.u64 = 0; return; }
    std::thread([fn, startContext, stackTop, basePtr]() {
        PPCContext tctx{};
        tctx.r1.u32 = stackTop;
        tctx.r13.u32 = HLE_AllocTlsThreadPointer();
        tctx.r3.u64 = startContext;
        try { fn(tctx, basePtr); }
        catch (const GuestThreadExit&) {}
        catch (...) { spdlog::error("[HLE] guest thread termino con excepcion"); }
        spdlog::info("[HLE] guest thread 0x{:08X} finalizado", startContext);
    }).detach();
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__ExTerminateThread) {
    SH_TRACK();
    spdlog::info("[HLE] ExTerminateThread code={}", ctx.r3.u32);
    throw GuestThreadExit{};
}

PPC_FUNC(__imp__NtResumeThread) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__NtSuspendThread) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__NtQueueApcThread) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__KeGetCurrentThread) { SH_TRACK(); ctx.r3.u64 = 0xF00D0001; }
PPC_FUNC(__imp__KeSetBasePriorityThread) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__KeQueryBasePriorityThread) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__KeSetAffinityThread) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__InterlockedPopEntrySList) { SH_TRACK(); ctx.r3.u64 = 0; }

// ============ Esperas / Eventos (no-op retornando exito) ============
PPC_FUNC(__imp__KeWaitForSingleObject) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__NtWaitForSingleObjectEx) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__KeWaitForMultipleObjects) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__NtWaitForMultipleObjectsEx) { SH_TRACK(); ctx.r3.u64 = 0; }

PPC_FUNC(__imp__NtCreateEvent) { SH_TRACK(); mem::WriteBE32(ctx.r3.u32, 0xE0000000 + s_nextHandle.fetch_add(1)); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__NtSetEvent) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__NtClearEvent) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__KeSetEvent) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__KeResetEvent) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__NtCreateMutant) { SH_TRACK(); mem::WriteBE32(ctx.r3.u32, 0xE1000000 + s_nextHandle.fetch_add(1)); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__NtReleaseMutant) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__NtCreateSemaphore) { SH_TRACK(); mem::WriteBE32(ctx.r3.u32, 0xE2000000 + s_nextHandle.fetch_add(1)); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__NtReleaseSemaphore) { SH_TRACK(); ctx.r3.u64 = 0; }

// ============ Objetos / XEX ============
PPC_FUNC(__imp__ObCreateSymbolicLink) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__ObDeleteSymbolicLink) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__ObDereferenceObject) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__ObReferenceObjectByHandle) { SH_TRACK(); ctx.r3.u64 = 0; }

PPC_FUNC(__imp__XexGetModuleHandle) {
    SH_TRACK();
    mem::WriteBE32(ctx.r4.u32, 1); // handle propio
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XexCheckExecutablePrivilege) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__RtlImageXexHeaderField) { SH_TRACK(); ctx.r3.u64 = 0; }

// ============ Debug / fatal ============
PPC_FUNC(__imp__DbgPrint) {
    SH_TRACK();
    spdlog::info("[DbgPrint] {}", mem::ReadString(ctx.r3.u32));
    ctx.r3.u64 = 0;
}
PPC_FUNC(__imp__DbgBreakPoint) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__KeBugCheck) { SH_TRACK(); spdlog::critical("[HLE] KeBugCheck!"); }
PPC_FUNC(__imp__KeBugCheckEx) {
    SH_TRACK();
    spdlog::critical("[HLE] KeBugCheckEx code=0x{:X} p1=0x{:X} p2=0x{:X} p3=0x{:X} p4=0x{:X}",
                     ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32);
}
PPC_FUNC(__imp__HalReturnToFirmware) {
    SH_TRACK();
    spdlog::critical("[HLE] HalReturnToFirmware({})", ctx.r3.u32);
    std::exit(0);
}
PPC_FUNC(__imp__RtlUnwind) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__RtlRaiseException) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__RtlCaptureContext) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp____C_specific_handler) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__KeEnableFpuExceptions) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__KiApcNormalRoutineNop) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__KeInitializeDpc) { SH_TRACK(); ctx.r3.u64 = 0; }
PPC_FUNC(__imp__ExRegisterTitleTerminateNotification) { SH_TRACK(); ctx.r3.u64 = 0; }

// ============ Stubs genericos ============

PPC_FUNC(__imp__XGetGameRegion) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XGetGameRegion");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamLoaderGetLaunchDataSize) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamLoaderGetLaunchDataSize");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XGetVideoMode) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XGetVideoMode");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamLoaderGetLaunchData) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamLoaderGetLaunchData");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamLoaderSetLaunchData) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamLoaderSetLaunchData");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamShowSigninUI) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamShowSigninUI");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamShowMarketplaceUI) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamShowMarketplaceUI");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamShowDeviceSelectorUI) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamShowDeviceSelectorUI");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamShowMessageBoxUI) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamShowMessageBoxUI");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamShowDirtyDiscErrorUI) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamShowDirtyDiscErrorUI");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamContentCreateEx) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamContentCreateEx");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamContentDelete) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamContentDelete");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamContentClose) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamContentClose");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamContentGetCreator) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamContentGetCreator");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamContentGetLicenseMask) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamContentGetLicenseMask");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamContentCreateEnumerator) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamContentCreateEnumerator");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamContentGetDeviceState) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamContentGetDeviceState");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamContentGetDeviceData) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamContentGetDeviceData");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XMsgStartIORequest) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XMsgStartIORequest");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamEnumerate) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamEnumerate");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamLoaderLaunchTitle) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamLoaderLaunchTitle");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamShowMessageBoxUIEx) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamShowMessageBoxUIEx");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XGetLanguage) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XGetLanguage");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XGetAVPack) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XGetAVPack");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamLoaderTerminateTitle) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamLoaderTerminateTitle");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__MmQueryAddressProtect) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: MmQueryAddressProtect");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlInitAnsiString) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: RtlInitAnsiString");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtOpenFile) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtOpenFile");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtCreateFile) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtCreateFile");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtWriteFile) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtWriteFile");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtQueryVirtualMemory) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtQueryVirtualMemory");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__MmQueryStatistics) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: MmQueryStatistics");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtClose) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtClose");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtSetInformationFile) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtSetInformationFile");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__FscSetCacheElementCount) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: FscSetCacheElementCount");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__ExGetXConfigSetting) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: ExGetXConfigSetting");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlUnicodeToMultiByteN) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: RtlUnicodeToMultiByteN");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlNtStatusToDosError) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: RtlNtStatusToDosError");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtQueryFullAttributesFile) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtQueryFullAttributesFile");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtQueryInformationFile) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtQueryInformationFile");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtQueryVolumeInformationFile) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtQueryVolumeInformationFile");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtQueryDirectoryFile) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtQueryDirectoryFile");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtReadFileScatter) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtReadFileScatter");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtReadFile) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtReadFile");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtDuplicateObject) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtDuplicateObject");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlFillMemoryUlong) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: RtlFillMemoryUlong");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlCompareMemoryUlong) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: RtlCompareMemoryUlong");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__KeReleaseSpinLockFromRaisedIrql) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: KeReleaseSpinLockFromRaisedIrql");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__KeAcquireSpinLockAtRaisedIrql) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: KeAcquireSpinLockAtRaisedIrql");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__KfReleaseSpinLock) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: KfReleaseSpinLock");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__KfAcquireSpinLock) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: KfAcquireSpinLock");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdEnableRingBufferRPtrWriteBack) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdEnableRingBufferRPtrWriteBack");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdInitializeRingBuffer) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdInitializeRingBuffer");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__MmGetPhysicalAddress) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: MmGetPhysicalAddress");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdSetSystemCommandBufferGpuIdentifierAddress) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdSetSystemCommandBufferGpuIdentifierAddress");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__sprintf) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: sprintf");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdGetCurrentDisplayGamma) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdGetCurrentDisplayGamma");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdEnableDisableClockGating) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdEnableDisableClockGating");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp___vsnprintf) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: _vsnprintf");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdQueryVideoMode) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdQueryVideoMode");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__KeLockL2) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: KeLockL2");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__KeUnlockL2) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: KeUnlockL2");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdShutdownEngines) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdShutdownEngines");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdGetCurrentDisplayInformation) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdGetCurrentDisplayInformation");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdSetDisplayMode) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdSetDisplayMode");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdSetGraphicsInterruptCallback) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdSetGraphicsInterruptCallback");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdInitializeEngines) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdInitializeEngines");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdIsHSIOTrainingSucceeded) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdIsHSIOTrainingSucceeded");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__MmFreePhysicalMemory) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: MmFreePhysicalMemory");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdPersistDisplay) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdPersistDisplay");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdSwap) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdSwap");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdGetSystemCommandBuffer) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdGetSystemCommandBuffer");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdQueryVideoFlags) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdQueryVideoFlags");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdCallGraphicsNotificationRoutines) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdCallGraphicsNotificationRoutines");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdInitializeScalerCommandBuffer) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdInitializeScalerCommandBuffer");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__MmAllocatePhysicalMemoryEx) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: MmAllocatePhysicalMemoryEx");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdRetrainEDRAM) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdRetrainEDRAM");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__VdRetrainEDRAMWorker) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: VdRetrainEDRAMWorker");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XNotifyPositionUI) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XNotifyPositionUI");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XNotifyGetNext) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XNotifyGetNext");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamInputGetCapabilities) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamInputGetCapabilities");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamInputGetState) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamInputGetState");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamInputSetState) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamInputSetState");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamUserGetSigninInfo) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamUserGetSigninInfo");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamUserGetName) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamUserGetName");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamUserGetSigninState) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamUserGetSigninState");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamGetSystemVersion) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamGetSystemVersion");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamUserCreateAchievementEnumerator) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamUserCreateAchievementEnumerator");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamUserGetXUID) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamUserGetXUID");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamEnableInactivityProcessing) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamEnableInactivityProcessing");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamResetInactivity) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamResetInactivity");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamNotifyCreateListener) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamNotifyCreateListener");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamGetExecutionId) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamGetExecutionId");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XAudioGetVoiceCategoryVolume) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XAudioGetVoiceCategoryVolume");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__MmMapIoSpace) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: MmMapIoSpace");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XMACreateContext) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XMACreateContext");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XMAReleaseContext) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XMAReleaseContext");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XAudioSubmitRenderDriverFrame) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XAudioSubmitRenderDriverFrame");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XAudioUnregisterRenderDriverClient) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XAudioUnregisterRenderDriverClient");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XAudioRegisterRenderDriverClient) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XAudioRegisterRenderDriverClient");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XAudioGetSpeakerConfig) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XAudioGetSpeakerConfig");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__MmQueryAllocationSize) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: MmQueryAllocationSize");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlMultiByteToUnicodeN) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: RtlMultiByteToUnicodeN");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlTimeToTimeFields) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: RtlTimeToTimeFields");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlFreeAnsiString) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: RtlFreeAnsiString");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlUnicodeStringToAnsiString) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: RtlUnicodeStringToAnsiString");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlInitUnicodeString) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: RtlInitUnicodeString");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NtFlushBuffersFile) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NtFlushBuffersFile");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__RtlTimeFieldsToTime) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: RtlTimeFieldsToTime");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XexGetProcedureAddress) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XexGetProcedureAddress");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__MmLockAndMapSegmentArray) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: MmLockAndMapSegmentArray");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__MmUnlockAndUnmapSegmentArray) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: MmUnlockAndUnmapSegmentArray");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp___snprintf) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: _snprintf");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_XNetStartup) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_XNetStartup");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_XNetGetTitleXnAddr) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_XNetGetTitleXnAddr");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamCreateEnumeratorHandle) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamCreateEnumeratorHandle");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XMsgStartIORequestEx) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XMsgStartIORequestEx");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XMsgInProcessCall) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XMsgInProcessCall");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamGetPrivateEnumStructureFromHandle) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamGetPrivateEnumStructureFromHandle");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamUserReadProfileSettings) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamUserReadProfileSettings");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamUserWriteProfileSettings) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamUserWriteProfileSettings");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamAlloc) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamAlloc");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamUserGetMembershipTierFromXUID) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamUserGetMembershipTierFromXUID");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XamUserGetOnlineCountryFromXUID) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XamUserGetOnlineCountryFromXUID");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__XNetLogonGetTitleID) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: XNetLogonGetTitleID");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_WSAStartup) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_WSAStartup");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_WSACleanup) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_WSACleanup");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_socket) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_socket");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_closesocket) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_closesocket");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_setsockopt) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_setsockopt");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_bind) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_bind");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_connect) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_connect");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_listen) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_listen");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_accept) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_accept");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_select) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_select");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_recv) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_recv");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_send) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_send");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll_inet_addr) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll_inet_addr");
    ctx.r3.u64 = 0;
}

PPC_FUNC(__imp__NetDll___WSAFDIsSet) {
    SH_TRACK();
    spdlog::warn("[HLE] Unimplemented import: NetDll___WSAFDIsSet");
    ctx.r3.u64 = 0;
}

