#include "libtemphumid.net.h"
#include <libtemphumid.h>
#include <stdlib.h>

using namespace libtemphumidnet;

libtemphumidnet::LibTempHumid::LibTempHumid()
{
	if (libth_init() != 0)
	{
		throw gcnew System::Exception("Unable to initialize libtemphumid");
	}
}

Collections::Generic::IList<Device^>^ libtemphumidnet::LibTempHumid::Devices::get()
{
	System::Collections::Generic::List<Device^> ^tret =
		gcnew System::Collections::Generic::List<Device^>();

	auto count = libth_get_devices(NULL, 0);
	auto buf = new libth_dev *[count];
	count = libth_get_devices(buf, count);

	for (size_t i = 0; i < count; i++)
	{
		auto cdev = gcnew Device(buf[i]);
		tret->Add(cdev);
	}

	delete buf;

	return gcnew System::Collections::ObjectModel::ReadOnlyCollection<Device^>(tret);
}

Device::Device(libth_dev *d)
{
	dev = d;
}

double Device::Temperature::get()
{
	return libth_get_temperature(this->dev);
}

double Device::Humidity::get()
{
	return libth_get_humidity(this->dev);
}

LibTempHumid::~LibTempHumid()
{
	libth_close();
}
