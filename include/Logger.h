#pragma once
#include <ntddk.h>
#include <wdf.h>

class Logger {
public:
    static NTSTATUS InitializeFileIoTarget(WDFDEVICE device);
    static NTSTATUS WriteToFile(const char* message);
    static void Log(const char* message);
    static void CloseFileHandle();

private:
    static WDFIOTARGET fileIoTarget;
};

