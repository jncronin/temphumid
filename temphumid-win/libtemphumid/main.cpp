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

#define IN_LIBTEMPHUMID
#include "libtemphumid.h"

#include <math.h>

static libusb_context *ctx = NULL;

int libth_init()
{
	auto ret = libusb_init(&ctx);

	return ret;
}

size_t libth_get_devices(libth_dev **devs, size_t devs_len)
{
	if (!ctx)
		return 0;

	/* Obtain and parse USB device list looking for VID=16c0, PID=27d8 and
		Serial=https://github.com/jncronin/temphumid */
	libusb_device **list;
	ssize_t dcount = libusb_get_device_list(ctx, &list);
	size_t cnt = 0;

	for (ssize_t i = 0; i < dcount; i++)
	{
		libusb_device *dev = list[i];
		libusb_device_descriptor desc;
		int ret = libusb_get_device_descriptor(list[i], &desc);

		if (ret == 0 && ((desc.idVendor == 0x16c0 && desc.idProduct == 0x27d9) ||
			(desc.idVendor == 0x03eb && desc.idProduct == 0x2402)))
		{
			unsigned char snum[512];
			libusb_device_handle *dev;

			ret = libusb_open(list[i], &dev);

			if (ret == 0)
			{
				auto len = libusb_get_string_descriptor_ascii(dev, desc.iSerialNumber,
					snum, 512);

				if (len > 0 && !strcmp("https://github.com/jncronin/temphumid", (const char *)snum))
				{
					if (cnt < devs_len && devs)
					{
						devs[cnt] = (libth_dev *)malloc(sizeof(libth_dev));
						devs[cnt]->dev = dev;
					}
					else
					{
						libusb_close(dev);
					}
					cnt++;
				}
				else
				{
					libusb_close(dev);
				}
			}
		}
	}
	return cnt;
}

double libth_get_temperature(libth_dev *dev)
{
	if (!dev || !dev->dev || !ctx)
		return NAN;

	int32_t val;

	int ret = libusb_control_transfer(dev->dev, libusb_request_type::LIBUSB_REQUEST_TYPE_CLASS |
		libusb_request_recipient::LIBUSB_RECIPIENT_INTERFACE |
		libusb_endpoint_direction::LIBUSB_ENDPOINT_IN,
		1, // HID_GET_REPORT
		1 << 8 | 1, // HID_REPORT_TYPE_INPUT, Report 1
		0,	// INTERFACE_NUMBER
		(unsigned char *)&val, 4, 0);

	if (ret == 4)
	{
		return (double)val / 100.0;
	}
	else
	{
		int32_t vals[2];
		ret = libusb_control_transfer(dev->dev, libusb_request_type::LIBUSB_REQUEST_TYPE_CLASS |
			libusb_request_recipient::LIBUSB_RECIPIENT_INTERFACE |
			libusb_endpoint_direction::LIBUSB_ENDPOINT_IN,
			1, // HID_GET_REPORT
			1 << 8, // HID_REPORT_TYPE_INPUT
			0,	// INTERFACE_NUMBER
			(unsigned char *)&vals, 8, 0);
		if (ret == 8)
		{
			return (double)vals[0] / 100.0;
		}
		return NAN;
	}
}

double libth_get_humidity(libth_dev *dev)
{
	if (!dev || !dev->dev || !ctx)
		return NAN;

	int32_t val;

	int ret = libusb_control_transfer(dev->dev, libusb_request_type::LIBUSB_REQUEST_TYPE_CLASS |
		libusb_request_recipient::LIBUSB_RECIPIENT_INTERFACE |
		libusb_endpoint_direction::LIBUSB_ENDPOINT_IN,
		1, // HID_GET_REPORT
		1 << 8 | 2, // HID_REPORT_TYPE_INPUT, Report 2
		0,	// INTERFACE_NUMBER
		(unsigned char *)&val, 4, 0);

	if (ret == 4)
	{
		return (double)val / 100.0;
	}
	else
	{
		int32_t vals[2];
		ret = libusb_control_transfer(dev->dev, libusb_request_type::LIBUSB_REQUEST_TYPE_CLASS |
			libusb_request_recipient::LIBUSB_RECIPIENT_INTERFACE |
			libusb_endpoint_direction::LIBUSB_ENDPOINT_IN,
			1, // HID_GET_REPORT
			1 << 8, // HID_REPORT_TYPE_INPUT
			0,	// INTERFACE_NUMBER
			(unsigned char *)&vals, 8, 0);
		if (ret == 8)
		{
			return (double)vals[1] / 100.0;
		}
		return NAN;
	}
}

void libth_close()
{
	if (ctx)
		libusb_exit(ctx);
}
