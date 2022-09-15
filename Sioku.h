//===-- Sioku.h - Simple IOKit-based USB library --------------------------===//
//
// Copyright (c) 2022 Jon Palmisciano
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <IOKit/usb/IOUSBLib.h>

#ifdef __cplusplus
extern "C" {
#endif

const uint32_t sioku_default_usb_timeout = 6;

//===----------------------------------------------------------------------===//

enum SiokuTransferState {
	SiokuTransferStateOK,
	SiokuTransferStateStall,
	SiokuTransferStateError,
};

enum SiokuTransferState sioku_transfer_state_from_error(IOReturn);

//===----------------------------------------------------------------------===//

typedef struct SiokuTransferResult SiokuTransferResult;
struct SiokuTransferResult {
	enum SiokuTransferState state;
	uint32_t length;
};

SiokuTransferResult sioku_transfer_result_create(IOReturn, uint32_t length);

//===----------------------------------------------------------------------===//

typedef struct SiokuClient SiokuClient;
struct SiokuClient {
	uint16_t vendor;
	uint16_t product;

	IOUSBDeviceInterface320** device;
	IOUSBInterfaceInterface300** interface;
	CFRunLoopSourceRef event_source;
};

SiokuClient* sioku_client_create(uint16_t vendor, uint16_t product);

bool sioku_client_open_device(SiokuClient*, io_service_t);
bool sioku_client_open_interface(SiokuClient*, uint8_t index, uint8_t alt_index);

bool sioku_client_connect(SiokuClient*, uint8_t index, uint8_t alt_index);
bool sioku_client_connect_default(SiokuClient*);

SiokuTransferResult sioku_client_transfer(SiokuClient*, uint8_t request_type,
    uint8_t request, uint16_t value, uint16_t index, void* data, size_t length);

SiokuTransferResult sioku_client_transfer_async(SiokuClient*, uint8_t request_type,
    uint8_t request, uint16_t value, uint16_t index, void* data, size_t length,
    uint32_t timeout);

void sioku_client_close_device(SiokuClient*);
void sioku_client_close_interface(SiokuClient*);

void sioku_client_disconnect(SiokuClient*);
bool sioku_client_reconnect(SiokuClient*);
bool sioku_client_reset(SiokuClient*);

#ifdef __cplusplus
}
#endif
