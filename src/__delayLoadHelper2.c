#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <intrin.h>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

EXTERN_C PVOID WINAPI __delayLoadHelper2(PCIMAGE_DELAYLOAD_DESCRIPTOR DelayloadDescriptor,
                                         PIMAGE_THUNK_DATA ThunkAddress)
{
    PVOID *p_ModuleHandle =
        (PVOID *)((uintptr_t)&__ImageBase + DelayloadDescriptor->ModuleHandleRVA);
    PVOID DllHandle = *p_ModuleHandle;
    if (!DllHandle) {
        PCSTR DllName = (PCSTR)((uintptr_t)&__ImageBase + DelayloadDescriptor->DllNameRVA);
        DllHandle = LoadLibraryExA(DllName, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!DllHandle)
            __ud2();  // __builtin_trap();

        PVOID ModuleHandle = _InterlockedCompareExchangePointer(p_ModuleHandle, DllHandle, NULL);
        if (ModuleHandle) {
            FreeLibrary((HMODULE)DllHandle);
            DllHandle = ModuleHandle;
        }
    }

    DWORD d = DelayloadDescriptor->ImportNameTableRVA - DelayloadDescriptor->ImportAddressTableRVA;
    PIMAGE_THUNK_DATA OriginalThunkAddress = (PIMAGE_THUNK_DATA)((uintptr_t)ThunkAddress + d);

    PCSTR ProcedureName;
    if (IMAGE_SNAP_BY_ORDINAL(OriginalThunkAddress->u1.Ordinal)) {
        ProcedureName = (PCSTR)IMAGE_ORDINAL(OriginalThunkAddress->u1.Ordinal);
    } else {
        PIMAGE_IMPORT_BY_NAME p = (PIMAGE_IMPORT_BY_NAME)((uintptr_t)&__ImageBase
                                                          + OriginalThunkAddress->u1.AddressOfData);
        ProcedureName = p->Name;
    }

    PVOID ProcedureAddress = (PVOID)GetProcAddress((HMODULE)DllHandle, ProcedureName);
    if (!ProcedureAddress)
        __ud2();  // __builtin_trap();

    ThunkAddress->u1.Function = (uintptr_t)ProcedureAddress;
    return ProcedureAddress;
}
