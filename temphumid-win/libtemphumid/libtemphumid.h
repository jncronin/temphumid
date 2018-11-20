#pragma once

#ifndef LIBTEMPHUMID_H
#define LIBTEMPHUMID_H

#ifdef IN_LIBTEMPHUMID
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

#include <libusb.h>

typedef struct _libth_dev
{
	libusb_device_handle *dev;
} libth_dev;
#else
typedef struct _libth_dev
{
	void *opaque;
} libth_dev;
#endif

#ifdef __cplusplus
extern "C" {
#endif
	int libth_init();
	size_t libth_get_devices(libth_dev **devs, size_t devs_len);
	double libth_get_temperature(libth_dev *dev);
	double libth_get_humidity(libth_dev *dev);
	void libth_close();
#ifdef __cplusplus
}
#endif

#endif
