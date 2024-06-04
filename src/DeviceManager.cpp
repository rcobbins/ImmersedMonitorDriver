#include <ntddk.h>
#include <wdf.h>
#include "../include/DeviceManager.h"
#include "../include/Logger.h"
#include <initguid.h>
#include <ntstrsafe.h>
#include <devpkey.h>

// Maximum number of monitors to track
#define MAX_MONITORS 10
#define MAX_DEVICE_ID_LEN 200

PVOID DeviceManager::RegistrationHandle = NULL;
UNICODE_STRING* DeviceManager::DisabledMonitors = NULL;
ULONG DeviceManager::DisabledMonitorCount = 0;

extern "C" {
    typedef PVOID DEVINST;
    typedef PVOID* PDEVINST;
    typedef const wchar_t* DEVINSTID_W;

    NTSTATUS CM_Get_Device_ID(
        DEVINST dnDevInst,
        PWCHAR Buffer,
        ULONG BufferLen,
        ULONG ulFlags
    ) {
        UNREFERENCED_PARAMETER(ulFlags);

        PDEVICE_OBJECT deviceObject = (PDEVICE_OBJECT)dnDevInst;
        if (deviceObject == NULL) {
            return STATUS_INVALID_PARAMETER;
        }

        ULONG requiredSize = 0;
        ULONG devPropType = DEVPROP_TYPE_STRING;
        NTSTATUS status = IoGetDevicePropertyData(deviceObject, &DEVPKEY_Device_InstanceId, 0, 0, BufferLen, Buffer, &requiredSize, &devPropType);

        return status;
    }

    NTSTATUS CM_Locate_DevNode(
        PDEVINST pdnDevInst,
        DEVINSTID_W pDeviceID,
        ULONG ulFlags
    ) {
        UNREFERENCED_PARAMETER(ulFlags);

        UNICODE_STRING deviceID;
        RtlInitUnicodeString(&deviceID, pDeviceID);

        PFILE_OBJECT fileObject;
        PDEVICE_OBJECT deviceObject;
        NTSTATUS status = IoGetDeviceObjectPointer(&deviceID, FILE_READ_DATA, &fileObject, &deviceObject);

        if (NT_SUCCESS(status)) {
            *pdnDevInst = (DEVINST)deviceObject;
        }

        return status;
    }

    NTSTATUS CM_Disable_DevNode(
        DEVINST dnDevInst,
        ULONG ulFlags
    ) {
        UNREFERENCED_PARAMETER(ulFlags);

        PDEVICE_OBJECT deviceObject = (PDEVICE_OBJECT)dnDevInst;
        if (deviceObject == NULL) {
            return STATUS_NO_SUCH_DEVICE;
        }

        IoInvalidateDeviceState(deviceObject);
        return STATUS_SUCCESS;
    }

    NTSTATUS CM_Enable_DevNode(
        DEVINST dnDevInst,
        ULONG ulFlags
    ) {
        UNREFERENCED_PARAMETER(ulFlags);

        PDEVICE_OBJECT deviceObject = (PDEVICE_OBJECT)dnDevInst;
        if (deviceObject == NULL) {
            return STATUS_NO_SUCH_DEVICE;
        }

        IoInvalidateDeviceState(deviceObject);
        return STATUS_SUCCESS;
    }
}

// Define GUIDs for kernel-mode if not available
DEFINE_GUID(GUID_DEVINTERFACE_MONITOR, 0xe6f07b5f, 0xee97, 0x4a90, 0xb0, 0xe6, 0x49, 0x30, 0x6e, 0xe1, 0x24, 0xaa);
DEFINE_GUID(GUID_DEVICE_INTERFACE_ARRIVAL, 0x7063e96e, 0x0861, 0x4f4e, 0x9a, 0xb2, 0xc1, 0x7e, 0xb6, 0xda, 0xe2, 0xb1);
DEFINE_GUID(GUID_DEVICE_INTERFACE_REMOVAL, 0x0a7c1f41, 0xd815, 0x4b82, 0xa3, 0x18, 0x67, 0x4a, 0xf2, 0xd1, 0x2d, 0xef);

NTSTATUS GetDeviceInstanceId(HANDLE hKey, UNICODE_STRING& deviceInstanceId) {
    // Query the device instance ID from the registry key
    WCHAR deviceInstanceIdBuffer[MAX_DEVICE_ID_LEN];
    RtlZeroMemory(deviceInstanceIdBuffer, sizeof(deviceInstanceIdBuffer)); // Ensure buffer is zeroed out
    UNICODE_STRING valueName;
    RtlInitUnicodeString(&valueName, L"DeviceInstanceID");

    ULONG resultLength;
    PKEY_VALUE_PARTIAL_INFORMATION pValueInfo = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(KEY_VALUE_PARTIAL_INFORMATION) + MAX_DEVICE_ID_LEN * sizeof(WCHAR), 'valK');
    if (!pValueInfo) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NTSTATUS status = ZwQueryValueKey(
        hKey,
        &valueName,
        KeyValuePartialInformation,
        pValueInfo,
        sizeof(KEY_VALUE_PARTIAL_INFORMATION) + MAX_DEVICE_ID_LEN * sizeof(WCHAR),
        &resultLength
    );

    if (NT_SUCCESS(status)) {
        RtlInitUnicodeString(&deviceInstanceId, (PWCHAR)pValueInfo->Data);
    }
    ExFreePool(pValueInfo);
    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS DeviceManager::GetSecondaryMonitorIds(
    _Out_writes_(maxMonitors) UNICODE_STRING* monitorIds,
    _In_ ULONG maxMonitors,
    _Out_ PULONG monitorCount
) {
    *monitorCount = 0;

    // Initialize all monitorIds entries
    for (ULONG i = 0; i < maxMonitors; i++) {
        RtlInitUnicodeString(&monitorIds[i], NULL);
    }

    UNICODE_STRING registryPath;
    RtlInitUnicodeString(&registryPath, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Enum\\DISPLAY");

    OBJECT_ATTRIBUTES objectAttributes;
    RtlZeroMemory(&objectAttributes, sizeof(OBJECT_ATTRIBUTES)); // Initialize objectAttributes
    InitializeObjectAttributes(&objectAttributes, &registryPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    HANDLE hKey = NULL;
    NTSTATUS status = ZwOpenKey(&hKey, KEY_READ, &objectAttributes);
    if (!NT_SUCCESS(status)) {
        Logger::Log("Failed to open registry key for monitors.");
        return status;
    }

    ULONG resultLength = 0;
    ULONG subKeysCount = 0; // Initialize subKeysCount to 0
    PKEY_FULL_INFORMATION keyInfo = (PKEY_FULL_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(KEY_FULL_INFORMATION), 'infK');
    if (!keyInfo) {
        ZwClose(hKey);
        Logger::Log("Failed to allocate memory for key information.");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = ZwQueryKey(hKey, KeyFullInformation, keyInfo, sizeof(KEY_FULL_INFORMATION), &resultLength);
    if (NT_SUCCESS(status)) {
        subKeysCount = keyInfo->SubKeys;
    }
    else {
        Logger::Log("Failed to query registry key.");
    }
    ExFreePool(keyInfo);
    if (!NT_SUCCESS(status)) {
        ZwClose(hKey);
        return status;
    }

    for (ULONG i = 0; i < subKeysCount && *monitorCount < maxMonitors; ++i) {
        // Open each subkey to read monitor information
        PKEY_BASIC_INFORMATION keyBasicInfo = (PKEY_BASIC_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(KEY_BASIC_INFORMATION) + 255 * sizeof(WCHAR), 'infK');
        if (!keyBasicInfo) {
            Logger::Log("Failed to allocate memory for key basic information.");
            continue;
        }

        status = ZwEnumerateKey(hKey, i, KeyBasicInformation, keyBasicInfo, sizeof(KEY_BASIC_INFORMATION) + 255 * sizeof(WCHAR), &resultLength);
        if (!NT_SUCCESS(status)) {
            ExFreePool(keyBasicInfo);
            continue;
        }

        UNICODE_STRING subKeyName;
        RtlZeroMemory(&subKeyName, sizeof(subKeyName)); // Initialize subKeyName
        subKeyName.Buffer = keyBasicInfo->Name;
        subKeyName.Length = (USHORT)keyBasicInfo->NameLength;
        subKeyName.MaximumLength = (USHORT)keyBasicInfo->NameLength;

        OBJECT_ATTRIBUTES subKeyAttributes;
        RtlZeroMemory(&subKeyAttributes, sizeof(OBJECT_ATTRIBUTES)); // Initialize subKeyAttributes
        InitializeObjectAttributes(&subKeyAttributes, &subKeyName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, hKey, NULL);

        HANDLE hSubKey = NULL;
        status = ZwOpenKey(&hSubKey, KEY_READ, &subKeyAttributes);
        if (NT_SUCCESS(status)) {
            UNICODE_STRING deviceInstanceId;
            RtlZeroMemory(&deviceInstanceId, sizeof(deviceInstanceId)); // Initialize deviceInstanceId
            status = GetDeviceInstanceId(hSubKey, deviceInstanceId);
            if (NT_SUCCESS(status)) {
                // Query the friendly name or other properties if necessary
                // For example, if (wcscmp(friendlyName, L"Primary Monitor") != 0)
                RtlCopyUnicodeString(&monitorIds[*monitorCount], &deviceInstanceId);
                (*monitorCount)++;
            }
            ZwClose(hSubKey);
        }

        ExFreePool(keyBasicInfo);
    }

    ZwClose(hKey);
    Logger::Log("Enumerated secondary monitors.");
    return STATUS_SUCCESS;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS DeviceManager::DisableMonitor(_In_ PUNICODE_STRING monitorId) {
    DEVINST devInst;
    NTSTATUS status = CM_Locate_DevNode(&devInst, monitorId->Buffer, 0);
    if (!NT_SUCCESS(status)) {
        Logger::Log("Failed to locate device node for monitor.");
        return status;
    }

    status = CM_Disable_DevNode(devInst, 0);
    if (!NT_SUCCESS(status)) {
        Logger::Log("Failed to disable monitor.");
        return status;
    }

    if (DisabledMonitorCount < MAX_MONITORS) {
        RtlCopyUnicodeString(&DisabledMonitors[DisabledMonitorCount++], monitorId);
    }

    Logger::Log("Disabled monitor.");
    return STATUS_SUCCESS;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS DeviceManager::EnableMonitor(_In_ PUNICODE_STRING monitorId) {
    DEVINST devInst;
    NTSTATUS status = CM_Locate_DevNode(&devInst, monitorId->Buffer, 0);
    if (!NT_SUCCESS(status)) {
        Logger::Log("Failed to locate device node for monitor.");
        return status;
    }

    status = CM_Enable_DevNode(devInst, 0);
    if (!NT_SUCCESS(status)) {
        Logger::Log("Failed to enable monitor.");
        return status;
    }

    for (ULONG i = 0; i < DisabledMonitorCount; ++i) {
        if (RtlEqualUnicodeString(&DisabledMonitors[i], monitorId, TRUE)) {
            for (ULONG j = i; j < DisabledMonitorCount - 1; ++j) {
                DisabledMonitors[j] = DisabledMonitors[j + 1];
            }
            --DisabledMonitorCount;
            break;
        }
    }

    Logger::Log("Enabled monitor.");
    return STATUS_SUCCESS;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS DeviceManager::BlockMonitorReconnection(WDFDEVICE device) {
    UNICODE_STRING altitude;
    RtlInitUnicodeString(&altitude, L"370010");

    NTSTATUS status = IoRegisterPlugPlayNotification(
        EventCategoryDeviceInterfaceChange,
        PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
        (PVOID)&GUID_DEVINTERFACE_MONITOR,
        WdfDriverWdmGetDriverObject(WdfDeviceGetDriver(device)),
        (PDRIVER_NOTIFICATION_CALLBACK_ROUTINE)DeviceChangeCallback,
        NULL,
        &RegistrationHandle
    );

    if (!NT_SUCCESS(status)) {
        Logger::Log("Failed to register for device interface change notifications.");
        return status;
    }

    Logger::Log("Blocking monitor reconnection...");
    return STATUS_SUCCESS;
}


_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS DeviceManager::UnblockMonitorReconnection() {
    if (RegistrationHandle != NULL) {
        IoUnregisterPlugPlayNotification(RegistrationHandle);
        RegistrationHandle = NULL;
    }

    Logger::Log("Unblocking monitor reconnection...");
    return STATUS_SUCCESS;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS DeviceManager::DeviceChangeCallback(_In_ PVOID NotificationStructure, _In_opt_ PVOID Context) {
    UNREFERENCED_PARAMETER(Context);

    PDEVICE_INTERFACE_CHANGE_NOTIFICATION notification = (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)NotificationStructure;
    if (IsEqualGUID(notification->Event, GUID_DEVICE_INTERFACE_ARRIVAL) ||
        IsEqualGUID(notification->Event, GUID_DEVICE_INTERFACE_REMOVAL)) {
        UNICODE_STRING deviceId;
        RtlInitUnicodeString(&deviceId, (PWSTR)notification->SymbolicLinkName->Buffer);
        for (ULONG i = 0; i < DisabledMonitorCount; ++i) {
            if (RtlEqualUnicodeString(&DisabledMonitors[i], &deviceId, TRUE)) {
                Logger::Log("Blocked reconnection attempt for monitor.");
                return STATUS_UNSUCCESSFUL;
            }
        }
    }
    return STATUS_SUCCESS;
}
