#include "optick.h"
#include "modengine/ext/profiling_extension.h"
#include "modengine/ext/profiling/main_loop.h"
#include "modengine/ext/profiling/thread_hooks.h"

namespace modengine::ext {

struct ProfilerPreludeData {
    void* ctx;
    const char* zone;
    uintptr_t original_address;
    uintptr_t original_return_address;
    uintptr_t profiler_entry_address;
    uintptr_t profiler_exit_address;
    uintptr_t profiler_prologue_address;
    void* rbx;
};

void ProfilingExtension::on_attach()
{
    info("Setting up profiler");

    hooked_MainLoop = register_hook(DS3, 0x140eccb30, tMainLoop);
    hooked_DLThreadHandler = register_hook(DS3, 0x1417ef4b0, tDLThreadHandler);
    install_profiler_zone(0x140ee4d10, "fun_140ee4d10");
    install_profiler_zone(0x140e95b35, "fun_140e95b35");
    install_profiler_zone(0x140f01cc0, "fun_140f01cc0");
    install_profiler_zone(0x140e96260, "fun_140e96260");
    install_profiler_zone(0x141779af0, "fun_141779af0");
    //install_profiler_zone(0x14007d5e0, "test_zone");

    // Test SprjGraphicsStep (don't these are actually called though?)
    install_profiler_zone(0x140d4d240, "SprjGraphicsStep::STEP_Init");
    install_profiler_zone(0x140d4e220, "SprjGraphicsStep::STEP_Prepare_forGraphicsFramework");
    install_profiler_zone(0x140d4d370, "SprjGraphicsStep::STEP_Initialize_forGraphicsFramework");
    install_profiler_zone(0x140d4e2a0, "SprjGraphicsStep::STEP_Prepare_forSystem");
    install_profiler_zone(0x140d4e0f0, "SprjGraphicsStep::STEP_Initialize_forSystem");
    install_profiler_zone(0x140d4e240, "SprjGraphicsStep::STEP_Prepare_forGuiFramework");
    install_profiler_zone(0x140d4d450, "SprjGraphicsStep::STEP_Initialize_forGuiFramework");
    install_profiler_zone(0x140d4e260, "SprjGraphicsStep::STEP_Prepare_forRenderingSystem");
    install_profiler_zone(0x140d4d4c0, "SprjGraphicsStep::STEP_Initialize_forRenderingSystem");
    install_profiler_zone(0x140d4e280, "SprjGraphicsStep::STEP_Prepare_forSfxSystem");
    install_profiler_zone(0x140d4e070, "SprjGraphicsStep::STEP_Initialize_forSfxSystem");
    install_profiler_zone(0x140d4e200, "SprjGraphicsStep::STEP_Prepare_forDecalSystem");
    install_profiler_zone(0x140d4d320, "SprjGraphicsStep::STEP_Initialize_forDecalSystem");
    install_profiler_zone(0x140d4c9d0, "SprjGraphicsStep::STEP_BeginDrawStep");
    install_profiler_zone(0x140d4e770, "SprjGraphicsStep::STEP_Update");
    install_profiler_zone(0x140d4e800, "SprjGraphicsStep::STEP_WaitFinishDrawStep");
    install_profiler_zone(0x140d4ca20, "SprjGraphicsStep::STEP_Finish");
}

void ProfilingExtension::on_detach()
{
}

void ProfilingExtension::install_profiler_zone(uintptr_t function_address, const char* zone)
{
    unsigned char prelude_code[] = {
        0x48, 0x89, 0x1d, 0xf1, 0xff, 0xff, 0xff, // mov qword ptr [rip-15], rbx
        0x48, 0x8D, 0x1D, 0xB2, 0xFF, 0xFF, 0xFF, // lea rbx, [rip-70]
        0xFF, 0x63, 0x20,                         // jmp [rbx+24]
    };

    unsigned char prologue_code[] = {
        0x48, 0x8D, 0x1D, 0xA8, 0xFF, 0xFF, 0xFF, // lea rbx, [rip-80]
        0xFF, 0x63, 0x28                          // jmp [rbx+48]
    };

    const auto prelude_data_len = sizeof(struct ProfilerPreludeData);
    const auto prelude_code_len = sizeof(prelude_code);
    const auto prologue_code_len = sizeof(prologue_code);

    void* prelude = VirtualAlloc(0, prelude_data_len + prelude_code_len + prologue_code_len, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    void* prelude_trampoline = (void*)((uintptr_t)prelude + prelude_data_len);
    void* prologue_trampoline = (void*)((uintptr_t)prelude_trampoline + prelude_code_len);

    auto hook = register_hook(ALL, function_address, prelude_trampoline);
    reapply();

    struct ProfilerPreludeData prelude_data = {
        nullptr,
        zone,
        (uintptr_t)hook->original,
        0,
        (uintptr_t)&profiler_zone,
        (uintptr_t)&profiler_zone_exit,
        (uintptr_t)prologue_trampoline,
        0x0,
    };

    memcpy(prologue_trampoline, prologue_code, prologue_code_len);
    memcpy(prelude, &prelude_data, prelude_data_len);
    memcpy(prelude_trampoline, prelude_code, prelude_code_len);
}

}

extern "C" void __profiler_end(void*)
{
    OPTICK_POP();
}

extern "C" void* __profiler_begin(const char* name)
{
    //spdlog::info("called");
    //OPTICK_EVENT(name);
    OPTICK_PUSH_DYNAMIC(name);
    return nullptr;
}
