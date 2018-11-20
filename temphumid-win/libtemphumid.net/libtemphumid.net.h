#pragma once

#include "libtemphumid.h"

using namespace System;

namespace libtemphumidnet {
	public ref class Device
	{
	private:
		libth_dev *dev;

	internal:
		Device(libth_dev *d);

	public:
		property double Temperature { double get(); }
		property double Humidity { double get(); }
	};

	public ref class LibTempHumid
	{
	private:
		~LibTempHumid();

	public:
		LibTempHumid();

		property Collections::Generic::IList<Device^>^ Devices
		{
			Collections::Generic::IList<Device^>^ get();
		}
	};
}
