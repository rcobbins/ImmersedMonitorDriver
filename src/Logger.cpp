#include "../include/Logger.h"

WDFIOTARGET Logger::fileIoTarget = NULL;

void Logger::Log(const char* message) {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "%s\n", message);
    WriteToFile(message);
}

NTSTATUS Logger::InitializeFileIoTarget(WDFDEVICE device) {
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    UNICODE_STRING fileName;
    NTSTATUS status;

    RtlInitUnicodeString(&fileName, L"\\??\\C:\\Temp\\DriverLog.txt");

    WDF_IO_TARGET_OPEN_PARAMS_INIT_CREATE_BY_NAME(
        &openParams,
        &fileName,
        GENERIC_WRITE | GENERIC_READ
    );

    status = WdfIoTargetCreate(
        device,
        WDF_NO_OBJECT_ATTRIBUTES,
        &fileIoTarget
    );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = WdfIoTargetOpen(fileIoTarget, &openParams);
    return status;
}

NTSTATUS Logger::WriteToFile(const char* message) {
    WDFREQUEST request;
    WDFMEMORY memory;
    NTSTATUS status;
    size_t messageLength = strlen(message);

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "File I/O attempted at incorrect IRQL level\n");
        return STATUS_INVALID_DEVICE_STATE;
    }

    status = WdfRequestCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        fileIoTarget,
        &request
    );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = WdfMemoryCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        NonPagedPoolNx,
        'Log',
        messageLength + 1,
        &memory,
        NULL
    );

    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(request);
        return status;
    }

    status = WdfMemoryCopyFromBuffer(memory, 0, (PVOID)message, messageLength + 1);

    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(request);
        WdfObjectDelete(memory);
        return status;
    }

    status = WdfIoTargetFormatRequestForWrite(
        fileIoTarget,
        request,
        memory,
        NULL,
        NULL
    );

    if (NT_SUCCESS(status)) {
        status = WdfRequestSend(request, fileIoTarget, WDF_NO_SEND_OPTIONS);
    }

    WdfObjectDelete(request);
    WdfObjectDelete(memory);

    return status;
}

void Logger::CloseFileHandle() {
    if (fileIoTarget != NULL) {
        WdfIoTargetClose(fileIoTarget);
        WdfObjectDelete(fileIoTarget);
        fileIoTarget = NULL;
    }
}
