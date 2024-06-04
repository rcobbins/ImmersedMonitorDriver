#pragma once
extern "C" {
#include <ntddk.h>
#include <wdf.h>
#include <ntstrsafe.h>
}

struct _EPROCESS;

// Context structure for the ProcessMonitor
typedef struct _PROCESS_MONITOR_CONTEXT {
    UNICODE_STRING processName;
    WDFDEVICE device;
    BOOLEAN running;
    WDFTIMER monitorTimer;
} PROCESS_MONITOR_CONTEXT, * PPROCESS_MONITOR_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PROCESS_MONITOR_CONTEXT, ProcessMonitorGetContext)

class ProcessMonitor {
public:
    ProcessMonitor(const UNICODE_STRING& processName, WDFDEVICE device);
    ~ProcessMonitor();

    NTSTATUS Initialize();
    NTSTATUS StartMonitoring();
    NTSTATUS StopMonitoring();
    static VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject);

private:
    static VOID MonitorLoop(WDFTIMER Timer);
    BOOLEAN IsProcessRunning() const;

    UNICODE_STRING processName;
    WDFDEVICE device;
    BOOLEAN running;
    WDFTIMER monitorTimer;

    static WDFDEVICE s_device;
};
