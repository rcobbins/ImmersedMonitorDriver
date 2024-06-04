#pragma once
#include <ntddk.h>
#include <wdf.h>

// DeviceManager class handles operations related to monitor devices.
class DeviceManager {
public:
    // Gets the IDs of secondary monitors.
    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        static NTSTATUS GetSecondaryMonitorIds(
            _Out_writes_(maxMonitors) UNICODE_STRING* monitorIds,
            _In_ ULONG maxMonitors,
            _Out_ PULONG monitorCount
        );

    // Disables a monitor given its ID.
    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        static NTSTATUS DisableMonitor(_In_ PUNICODE_STRING monitorId);

    // Enables a monitor given its ID.
    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        static NTSTATUS EnableMonitor(_In_ PUNICODE_STRING monitorId);

    // Blocks attempts to reconnect disabled monitors.
    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        static NTSTATUS BlockMonitorReconnection(WDFDEVICE device);

    // Unblocks attempts to reconnect disabled monitors.
    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        static NTSTATUS UnblockMonitorReconnection();

private:
    // Handle for the registration of plug and play notifications.
    static PVOID RegistrationHandle;

    // Array to track disabled monitor IDs.
    static UNICODE_STRING* DisabledMonitors;

    // Count of disabled monitors.
    static ULONG DisabledMonitorCount;

    // Callback for device change notifications.
    _Must_inspect_result_
        _IRQL_requires_max_(PASSIVE_LEVEL)
        static NTSTATUS DeviceChangeCallback(
            _In_ PVOID NotificationStructure,
            _In_opt_ PVOID Context
        );
};
