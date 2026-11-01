/* lzham_winxp_compat.c - Windows XP compatibility shim.
 *
 * Windows XP has no GetThreadId() (it appeared in Vista). MinGW's libgcc contains gthr-win32.o,
 * whose __gthread_equal() compares thread ids with GetThreadId(); any C++ translation unit with
 * static constructors pulls that object in, so the resulting DLL/EXE imports GetThreadId and XP
 * refuses to load it with STATUS_ENTRYPOINT_NOT_FOUND (0xC0000139).
 *
 * The codec never calls __gthread_equal() (it does not use std::thread; its own threading uses
 * Win32 threads or pthreads directly), so satisfying the import locally is enough: the import
 * disappears and XP can load the binary. If the helper were ever called, GetCurrentThreadId() is
 * the right answer for the only case that matters here (the current thread).
 *
 * Only built for the LZHAM_TARGET_WINDOWS_XP configuration, which is 32-bit only.
 */
#if defined(_WIN32) && !defined(_WIN64)

#include <windows.h>

static DWORD WINAPI lzham_get_thread_id_stub(HANDLE thread)
{
   (void)thread;
   return GetCurrentThreadId();
}

/* gthr-win32.o references the import pointer, not the function, so that is the symbol to define.
   The @4 suffix is the stdcall decoration on 32-bit Windows (one HANDLE argument). */
DWORD (WINAPI *lzham_get_thread_id_import)(HANDLE) __asm__("__imp__GetThreadId@4") = lzham_get_thread_id_stub;

#endif /* _WIN32 && !_WIN64 */
