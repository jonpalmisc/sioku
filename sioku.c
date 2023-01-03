//
//  Copyright (c) 2022-2023 Jon Palmisciano. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//  3. Neither the name of the copyright holder nor the names of its
//     contributors may be used to endorse or promote products derived from this
//     software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.
//

#include "sioku.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>

static void delay(unsigned ms)
{
    struct timespec duration;
    duration.tv_sec = ms / 1000;
    duration.tv_nsec = (ms % 1000) * 1000000L;

    nanosleep(&duration, NULL);
}

static void CFDictionarySetShort(CFMutableDictionaryRef dict, const void *key, uint16_t value)
{
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &value);
    if (number == NULL)
        return;

    CFDictionarySetValue(dict, key, number);
    CFRelease(number);
}

static bool IOQueryInterface(io_service_t service, CFUUIDRef pluginType,
    CFUUIDRef interfaceType, LPVOID *interface)
{
    bool ok = false;

    SInt32 score;
    IOCFPlugInInterface **plugin;
    if (IOCreatePlugInInterfaceForService(service, pluginType, kIOCFPlugInInterfaceID, &plugin, &score) != kIOReturnSuccess)
        goto exit;

    ok = (*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(interfaceType), interface) == kIOReturnSuccess;
    IODestroyPlugInInterface(plugin);
exit:
    IOObjectRelease(service);
    return ok;
}

SiokuTransferState sioku_transfer_state_from_iokit(IOReturn error)
{
    // Some non-success errors, e.g. an aborted transfer, are expected and
    // are not indicative of overall failure. As such, these errors are
    // mapped to a successful transfer state.
    switch (error) {
    case kIOReturnSuccess:
    case kIOReturnAborted:
    case kIOReturnTimeout:
    case kIOUSBTransactionTimeout:
        return SiokuTransferStateOk;
    case kIOUSBPipeStalled:
        return SiokuTransferStateStall;
    default:
        return SiokuTransferStateError;
    }
}

static const SiokuTransferResult error_transfer_result = {
    .state = SiokuTransferStateError,
    .length = UINT32_MAX
};

SiokuTransferResult sioku_transfer_result(IOReturn error, uint32_t length)
{
    SiokuTransferResult result = {
        .state = sioku_transfer_state_from_iokit(error),
        .length = length,
    };

    return result;
}

SiokuClient *sioku_client_create(uint16_t vendor, uint16_t product)
{
    SiokuClient *client = (SiokuClient *)malloc(sizeof(SiokuClient));

    client->vendor = vendor;
    client->product = product;
    client->device = NULL;
    client->interface = NULL;
    client->event_source = NULL;

    return client;
}

bool sioku_open_device(SiokuClient *client, io_service_t service)
{
    IOUSBConfigurationDescriptorPtr config;

    if (!IOQueryInterface(service, kIOUSBDeviceUserClientTypeID, kIOUSBDeviceInterfaceID320, (LPVOID *)&client->device))
        return false;

    if ((*client->device)->USBDeviceOpenSeize(client->device) != kIOReturnSuccess)
        goto cleanup;
    if ((*client->device)->GetConfigurationDescriptorPtr(client->device, 0, &config) != kIOReturnSuccess)
        goto fail;
    if ((*client->device)->SetConfiguration(client->device, config->bConfigurationValue) != kIOReturnSuccess)
        goto fail;
    if ((*client->device)->CreateDeviceAsyncEventSource(client->device, &client->event_source) != kIOReturnSuccess)
        goto fail;

    CFRunLoopAddSource(CFRunLoopGetCurrent(), client->event_source, kCFRunLoopDefaultMode);
    return true;
fail:
    (*client->device)->USBDeviceClose(client->device);
cleanup:
    (*client->device)->Release(client->device);
    return false;
}

bool sioku_open_interface(SiokuClient *client, uint8_t index, uint8_t alt_index)
{
    IOUSBFindInterfaceRequest request;
    request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    request.bAlternateSetting = kIOUSBFindInterfaceDontCare;
    request.bInterfaceClass = kIOUSBFindInterfaceDontCare;

    io_iterator_t it;
    if ((*client->device)->CreateInterfaceIterator(client->device, &request, &it) != kIOReturnSuccess)
        return false;

    io_service_t service = IO_OBJECT_NULL;
    for (size_t i = 0; i <= index; ++i) {
        if ((service = IOIteratorNext(it)) == IO_OBJECT_NULL)
            continue;
        if (i == index)
            break;

        IOObjectRelease(service);
    }

    IOObjectRelease(it);

    if (service == IO_OBJECT_NULL)
        return false;
    if (!IOQueryInterface(service, kIOUSBInterfaceUserClientTypeID, kIOUSBInterfaceInterfaceID300, (LPVOID *)&client->interface))
        return false;
    if ((*client->interface)->USBInterfaceOpenSeize(client->interface) != kIOReturnSuccess)
        return false;
    if (alt_index != 1 || (*client->interface)->SetAlternateInterface(client->interface, alt_index) == kIOReturnSuccess)
        return true;

    (*client->interface)->USBInterfaceClose(client->interface);
    (*client->interface)->Release(client->interface);

    return false;
}

static const uint32_t wait_retry_timeout = 200;

bool sioku_connect(SiokuClient *client, uint8_t index, uint8_t alt_index)
{
    CFMutableDictionaryRef matches;
    const char *device_class = kIOUSBDeviceClassName;
    io_iterator_t it;
    io_service_t service;
    bool success = false;

loop:
    matches = IOServiceMatching(device_class);
    if (matches == NULL)
        goto end;

    CFDictionarySetShort(matches, CFSTR(kUSBVendorID), client->vendor);
    CFDictionarySetShort(matches, CFSTR(kUSBProductID), client->product);
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matches, &it) != kIOReturnSuccess)
        goto retry;

    while ((service = IOIteratorNext(it)) != IO_OBJECT_NULL) {
        if (!sioku_open_device(client, service)) {
            IOObjectRelease(service);
            continue;
        }

        if (!sioku_open_interface(client, index, alt_index)) {
            sioku_close_device(client);
            continue;
        }

        // Break rather than return so that the iterator object can
        // still be disposed of.
        success = true;
        break;
    }

    IOObjectRelease(it);

    // Stop searching if the requested device has been found.
    if (success)
        goto end;
retry:
    delay(wait_retry_timeout);
    goto loop;
end:
    return success;
}

bool sioku_connect_default(SiokuClient *client)
{
    return sioku_connect(client, 0, 0);
}

static char g_null_buffer[0x1000] = { 0 };

SiokuTransferResult sioku_transfer(SiokuClient *client, uint8_t request_type, uint8_t request,
    uint16_t value, uint16_t index, void *data, size_t length)
{
    // Use the global null buffer if no data pointer is passed in but a
    // non-zero length is specified.
    if (length > 0 && data == NULL) {
        memset(g_null_buffer, 0, sizeof g_null_buffer);
        data = &g_null_buffer;
    }

    IOUSBDevRequestTO r;
    r.wLenDone = 0;
    r.pData = data;
    r.bRequest = request;
    r.bmRequestType = request_type;
    r.wLength = OSSwapLittleToHostInt16(length);
    r.wValue = OSSwapLittleToHostInt16(value);
    r.wIndex = OSSwapLittleToHostInt16(index);
    r.completionTimeout = sioku_default_usb_timeout;
    r.noDataTimeout = sioku_default_usb_timeout;

    IOReturn error = (*client->device)->DeviceRequestTO(client->device, &r);

    return sioku_transfer_result(error, r.wLenDone);
}

static void async_transfer_callback(void *object, IOReturn error, void *arg)
{
    SiokuTransferResult *result = (SiokuTransferResult *)object;
    if (result == NULL)
        goto stop;

    memcpy(&result->length, &arg, sizeof(result->length));
    result->state = sioku_transfer_state_from_iokit(error);
stop:
    CFRunLoopStop(CFRunLoopGetCurrent());
}

SiokuTransferResult sioku_transfer_async(SiokuClient *client, uint8_t request_type, uint8_t request,
    uint16_t value, uint16_t index, void *data, size_t length, uint32_t timeout)
{
    // Use the global null buffer if no data pointer is passed in but a
    // non-zero length is specified.
    if (length > 0 && data == NULL) {
        memset(g_null_buffer, 0, sizeof g_null_buffer);
        data = &g_null_buffer;
    }

    IOUSBDevRequestTO r;
    r.wLenDone = 0;
    r.pData = data;
    r.bRequest = request;
    r.bmRequestType = request_type;
    r.wLength = OSSwapLittleToHostInt16(length);
    r.wValue = OSSwapLittleToHostInt16(value);
    r.wIndex = OSSwapLittleToHostInt16(index);
    r.completionTimeout = sioku_default_usb_timeout;
    r.noDataTimeout = sioku_default_usb_timeout;

    SiokuTransferResult result;
    IOReturn error = (*client->device)->DeviceRequestAsyncTO(client->device, &r, async_transfer_callback, &result);
    if (error != kIOReturnSuccess)
        return error_transfer_result;

    delay(timeout);

    error = (*client->device)->USBDeviceAbortPipeZero(client->device);
    if (error != kIOReturnSuccess)
        return error_transfer_result;

    CFRunLoopRun();

    return result;
}

void sioku_close_device(SiokuClient *client)
{
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), client->event_source, kCFRunLoopDefaultMode);
    CFRelease(client->event_source);

    (*client->device)->USBDeviceClose(client->device);
    (*client->device)->Release(client->device);
}

void sioku_close_interface(SiokuClient *client)
{
    (*client->interface)->USBInterfaceClose(client->interface);
    (*client->interface)->Release(client->interface);
}

void sioku_disconnect(SiokuClient *client)
{
    sioku_close_interface(client);
    sioku_close_device(client);
}

bool sioku_reconnect(SiokuClient *client)
{
    return sioku_reset(client) && sioku_connect_default(client);
}

bool sioku_reset(SiokuClient *client)
{
    return (*client->device)->ResetDevice(client->device) == kIOReturnSuccess
        && (*client->device)->USBDeviceReEnumerate(client->device, 0) == kIOReturnSuccess;
}
