//
//  sioku.h
//  https://github.com/jonpalmisc/sioku
//
//  Copyright (c) 2022-2023 Jon Palmisciano. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
//  3. Neither the name of the copyright holder nor the names of its
//     contributors may be used to endorse or promote products derived from
//     this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.
//

#pragma once

#include <IOKit/usb/IOUSBLib.h>

#ifdef __cplusplus
extern "C" {
#endif

const uint32_t SIOKU_DEFAULT_USB_TIMEOUT = 6;

typedef enum {
  SiokuTransferStateOk,
  SiokuTransferStateStall,
  SiokuTransferStateError,
} SiokuTransferState;

SiokuTransferState sioku_transfer_state_from_iokit(IOReturn error);

typedef struct {
  SiokuTransferState state;
  uint32_t length;
} SiokuTransferResult;

SiokuTransferResult sioku_transfer_result(IOReturn error, uint32_t length);

typedef struct {
  uint16_t vendor;
  uint16_t product;

  IOUSBDeviceInterface320 **device;
  IOUSBInterfaceInterface300 **interface;
  CFRunLoopSourceRef event_source;
} SiokuClient;

SiokuClient *sioku_client_create(uint16_t vendor, uint16_t product);

bool sioku_open_device(SiokuClient *client, io_service_t service);
bool sioku_open_interface(SiokuClient *client, uint8_t index,
                          uint8_t alt_index);

bool sioku_connect(SiokuClient *client, uint8_t index, uint8_t alt_index);
bool sioku_connect_default(SiokuClient *client);

SiokuTransferResult sioku_transfer(SiokuClient *client, uint8_t request_type,
                                   uint8_t request, uint16_t value,
                                   uint16_t index, void *data, size_t length);
SiokuTransferResult sioku_transfer_async(SiokuClient *client,
                                         uint8_t request_type, uint8_t request,
                                         uint16_t value, uint16_t index,
                                         void *data, size_t length,
                                         uint32_t timeout);

void sioku_close_device(SiokuClient *client);
void sioku_close_interface(SiokuClient *client);

void sioku_disconnect(SiokuClient *client);
bool sioku_reconnect(SiokuClient *client);
bool sioku_reset(SiokuClient *client);

#ifdef __cplusplus
}
#endif
