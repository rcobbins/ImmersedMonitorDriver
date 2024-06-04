#include <ntddk.h>
#include <wdf.h>
#include <sddl.h>
#include "../include/ProcessMonitor.h"
#include "../include/Logger.h"

#define IOCTL_START_MONITORING CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_STOP_MONITORING CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

WDFDEVICE g_Device; // Global variable to hold the device handle
ProcessMonitor* g_Monitor; // Global variable to hold the ProcessMonitor instance
VOID EvtIoDeviceControl(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t OutputBufferLength, _In_ size_t InputBufferLength, _In_ ULONG IoControlCode);

VOID EvtIoDeviceControl(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t OutputBufferLength, _In_ size_t InputBufferLength, _In_ ULONG IoControlCode) {
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    NTSTATUS status = STATUS_SUCCESS;
    size_t bytesReturned = 0;

    switch (IoControlCode) {
    case IOCTL_START_MONITORING:
        // Your code to start monitoring
        if (g_Monitor) {
            status = g_Monitor->StartMonitoring();
            if (!NT_SUCCESS(status)) {
                // Handle error
            }
        }
        else {
            // Handle error due to uninitialized g_Monitor
            status = STATUS_UNSUCCESSFUL;
        }
        break;

    case IOCTL_STOP_MONITORING:
        // Your code to stop monitoring
        if (g_Monitor) {
            status = g_Monitor->StopMonitoring();
            if (!NT_SUCCESS(status)) {
                // Handle error
            }
        }
        else {
            // Handle error due to uninitialized g_Monitor
            status = STATUS_UNSUCCESSFUL;
        }
        break;

    default:
        // Handle unrecognized IOCTL codes
        status = STATUS_INVALID_PARAMETER;
        break;
    }

    // Complete the request
    WdfRequestCompleteWithInformation(Request, status, bytesReturned);
}


extern "C" NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
) {
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDFDRIVER driver;
    EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControl;
    WDFDEVICE_INIT* pInit = NULL;
    UNICODE_STRING sddlString;
//    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES monitorAttributes;
//    WDFMEMORY monitorMemory;
    WDF_OBJECT_ATTRIBUTES_INIT(&monitorAttributes);

    // Initialize the driver configuration structure
    WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);

    // Create the WDF driver object
    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, &driver);
    if (!NT_SUCCESS(status)) {
        Logger::Log("WdfDriverCreate failed");
        return status;
    }

    // Assign the SDDL string to the device initialization structure
    RtlInitUnicodeString(&sddlString, L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");

    // Allocate a WDFDEVICE_INIT structure
    pInit = WdfControlDeviceInitAllocate(driver, &sddlString);
    if (pInit == NULL) {
        Logger::Log("WdfControlDeviceInitAllocate failed");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = WdfDeviceInitAssignSDDLString(pInit, &sddlString);
    if (!NT_SUCCESS(status)) {
        Logger::Log("WdfDeviceInitAssignSDDLString failed");
        WdfDeviceInitFree(pInit);
        return status;
    }

    WDF_IO_QUEUE_CONFIG ioQueueConfig;
    WDFQUEUE queue;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchSequential);
    ioQueueConfig.EvtIoDeviceControl = EvtIoDeviceControl;

    status = WdfIoQueueCreate(g_Device, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
    if (!NT_SUCCESS(status)) {
        Logger::Log("WdfIoQueueCreate failed");
        WdfDeviceInitFree(pInit);
        return status;
    }

    // Create the device
    status = WdfDeviceCreate(&pInit, WDF_NO_OBJECT_ATTRIBUTES, &g_Device);
    if (!NT_SUCCESS(status)) {
        Logger::Log("WdfDeviceCreate failed");
        WdfDeviceInitFree(pInit);
        return status;
    }

    // Initialize the logger
    status = Logger::InitializeFileIoTarget(g_Device);
    if (!NT_SUCCESS(status)) {
        Logger::Log("Logger::InitializeFileIoTarget failed");
        WdfDeviceInitFree(pInit);
        return status;
    }

    Logger::Log("ImmersedMonitorManager driver loaded.");

    monitorAttributes.ParentObject = g_Device;

    status = WdfObjectAllocateContext(g_Device, &monitorAttributes, (PVOID*)&g_Monitor);
    if (!NT_SUCCESS(status)) {
        Logger::Log("WdfObjectAllocateContext failed for ProcessMonitor");
        WdfDeviceInitFree(pInit);
        return status;
    }

    UNICODE_STRING processName;
    RtlInitUnicodeString(&processName, L"Immersed.exe");
    g_Monitor->Initialize();

    status = g_Monitor->StartMonitoring();
    if (!NT_SUCCESS(status)) {
        Logger::Log("Failed to start process monitoring.");
        WdfDeviceInitFree(pInit);
        return status;
    }

    // Set the DriverUnload routine
    DriverObject->DriverUnload = [](PDRIVER_OBJECT DriverObject) {
        UNREFERENCED_PARAMETER(DriverObject);

        g_Monitor->StopMonitoring();
        Logger::Log("ImmersedMonitorManager driver unloaded.");

        // Close the logger file handle
        Logger::CloseFileHandle();
    };

    return STATUS_SUCCESS;
}
