#include "../include/ProcessMonitor.h"
#include "../include/Logger.h"
#include "../include/DeviceManager.h"

WDFDEVICE ProcessMonitor::s_device;

// External kernel API for looking up a process by its process ID.
extern "C" {
    NTKERNELAPI NTSTATUS PsLookupProcessByProcessId(
        HANDLE ProcessId,
        PEPROCESS* Process
    );
}

// Constructor for the ProcessMonitor class.
ProcessMonitor::ProcessMonitor(const UNICODE_STRING& processName, WDFDEVICE device)
    : processName(processName), device(device), running(FALSE), monitorTimer(NULL) {
    s_device = device; // Initialize the static device handle
}

NTSTATUS ProcessMonitor::Initialize() {
    PPROCESS_MONITOR_CONTEXT context = ProcessMonitorGetContext(this->device);
    RtlInitUnicodeString(&context->processName, this->processName.Buffer);
    context->device = device;
    context->running = FALSE;
    context->monitorTimer = NULL;

    return STATUS_SUCCESS;
}

// Destructor for the ProcessMonitor class.
ProcessMonitor::~ProcessMonitor() {
    StopMonitoring();
}

// Starts the monitoring process.
NTSTATUS ProcessMonitor::StartMonitoring() {
    WDF_TIMER_CONFIG timerConfig;
    WDF_OBJECT_ATTRIBUTES timerAttributes;
    NTSTATUS status;

    WDF_TIMER_CONFIG_INIT_PERIODIC(&timerConfig, MonitorLoop, 1000); // 1 second period
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = device;

    status = WdfTimerCreate(
        &timerConfig,
        &timerAttributes,
        &monitorTimer
    );

    if (NT_SUCCESS(status)) {
        WdfTimerStart(monitorTimer, WDF_REL_TIMEOUT_IN_MS(1000));
        running = TRUE;
        Logger::Log("Started monitoring process.");
    }
    else {
        Logger::Log("Failed to start monitoring timer.");
    }

    return status;
}

// Stops the monitoring process.
NTSTATUS ProcessMonitor::StopMonitoring() {
    if (monitorTimer) {
        WdfTimerStop(monitorTimer, TRUE);
        WdfObjectDelete(monitorTimer);
        monitorTimer = NULL;
        running = FALSE;
    }

    Logger::Log("Stopped monitoring process.");
    return STATUS_SUCCESS;
}

// The main loop for monitoring the process.
VOID ProcessMonitor::MonitorLoop(WDFTIMER Timer) {
    ProcessMonitor* monitor = static_cast<ProcessMonitor*>(WdfTimerGetParentObject(Timer));
    BOOLEAN immersedRunning = FALSE;

    if (monitor->running) {
        BOOLEAN isRunning = monitor->IsProcessRunning();

        if (isRunning && !immersedRunning) {
            immersedRunning = TRUE;
            UNICODE_STRING monitorIds[10];
            ULONG monitorIdCount = 0;
            NTSTATUS status = DeviceManager::GetSecondaryMonitorIds(monitorIds, 10, &monitorIdCount);

            if (NT_SUCCESS(status)) {
                for (ULONG i = 0; i < monitorIdCount; ++i) {
                    DeviceManager::DisableMonitor(&monitorIds[i]);
                }
                DeviceManager::BlockMonitorReconnection(s_device); // Pass static device
                Logger::Log("Immersed agent detected, secondary monitors disabled.");
            }
        }
        else if (!isRunning && immersedRunning) {
            immersedRunning = FALSE;
            DeviceManager::UnblockMonitorReconnection();
            UNICODE_STRING monitorIds[10];
            ULONG monitorIdCount = 0;
            NTSTATUS status = DeviceManager::GetSecondaryMonitorIds(monitorIds, 10, &monitorIdCount);

            if (NT_SUCCESS(status)) {
                for (ULONG i = 0; i < monitorIdCount; ++i) {
                    DeviceManager::EnableMonitor(&monitorIds[i]);
                }
                Logger::Log("Immersed agent closed, secondary monitors re-enabled.");
            }
        }
    }
}

// Checks if the specified process is currently running.
BOOLEAN ProcessMonitor::IsProcessRunning() const {
    PEPROCESS eProcess;
    NTSTATUS status = PsLookupProcessByProcessId(UlongToHandle(4), &eProcess);  // Assuming PID 4 for demonstration

    if (NT_SUCCESS(status)) {
        // Implement your process check logic here
        ObDereferenceObject(eProcess);  // Dereference the process object after use
        return TRUE;
    }

    return FALSE;
}
