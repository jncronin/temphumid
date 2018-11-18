/*
	Copyright (c) 2018  John Cronin

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include "pch.h"
#include <libusb.h>
#include <iostream>

int main()
{
	libusb_context *ctx;

	auto ret = libusb_init(&ctx);

	/* Obtain and parse USB device list looking for VID=16c0, PID=27d8 and
		Serial=https://github.com/jncronin/temphumid */
	libusb_device **list;
	auto dcount = libusb_get_device_list(ctx, &list);

	libusb_device_handle *dev = NULL;
	for (auto i = 0; i < dcount; i++)
	{
		libusb_device_descriptor desc;
		ret = libusb_get_device_descriptor(list[i], &desc);

		if (ret == 0 && desc.idVendor == 0x16c0 && desc.idProduct == 0x27d8)
		{
			unsigned char snum[512];

			ret = libusb_open(list[i], &dev);

			if (ret == 0)
			{
				auto len = libusb_get_string_descriptor_ascii(dev, desc.iSerialNumber,
					snum, 512);

				if (len > 0 && !strcmp("https://github.com/jncronin/temphumid", (const char *)snum))
				{
					break;
				}

				libusb_close(dev);
			}
		}
	}

	libusb_free_device_list(list, 0);

	if (dev)
	{
		// We have the device, send a control transfer
		while (true)
		{
			uint32_t vals[2];

			ret = libusb_control_transfer(dev, libusb_request_type::LIBUSB_REQUEST_TYPE_VENDOR |
				libusb_endpoint_direction::LIBUSB_ENDPOINT_IN, 1, 0, 0,
				(unsigned char *)&vals[0], 8, 0);

			if (ret == 8)
			{
				auto temp = (double)vals[0] / 100.0;
				auto humid = (double)vals[1] / 100.0;
				std::cout << "Temp: " << temp << " C   Humid: " << humid << " %" << std::endl;
				Sleep(1000);
			}
			else
			{
				std::cout << "Bad response from device" << std::endl;
			}
		}
	}
	else
	{
		std::cout << "Unable to find TempHumid device" << std::endl;
	}

	libusb_exit(ctx);
}
