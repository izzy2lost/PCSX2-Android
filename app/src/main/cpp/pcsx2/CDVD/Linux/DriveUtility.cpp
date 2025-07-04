// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "CDVD/CDVDdiscReader.h"

#include <libudev.h>
#include <linux/cdrom.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

std::vector<std::string> GetOpticalDriveList()
{
#if defined(__linux__) && !defined(__ANDROID__)
	udev* udev_context = udev_new();
	if (!udev_context)
		return {};

	std::vector<std::string> drives;
	udev_enumerate* enumerate = udev_enumerate_new(udev_context);
	if (enumerate)
	{
		udev_enumerate_add_match_subsystem(enumerate, "block");
		udev_enumerate_add_match_property(enumerate, "ID_CDROM_DVD", "1");
		udev_enumerate_scan_devices(enumerate);
		udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);

		udev_list_entry* dev_list_entry;
		udev_list_entry_foreach(dev_list_entry, devices)
		{
			const char* path = udev_list_entry_get_name(dev_list_entry);
			udev_device* device = udev_device_new_from_syspath(udev_context, path);
			const char* devnode = udev_device_get_devnode(device);
			if (devnode)
				drives.push_back(devnode);
			udev_device_unref(device);
		}
		udev_enumerate_unref(enumerate);
	}
	udev_unref(udev_context);

	return drives;
#else
    return {};
#endif
}

void GetValidDrive(std::string& drive)
{
	if (!drive.empty())
	{
#if defined(__linux__) && !defined(__ANDROID__)
		int fd = open(drive.c_str(), O_RDONLY | O_NONBLOCK);
		if (fd != -1)
		{
			if (ioctl(fd, CDROM_GET_CAPABILITY, 0) == -1)
				drive.clear();
			close(fd);
		}
		else
		{
			drive.clear();
		}
#else
        drive.clear();
#endif
	}
	if (drive.empty())
	{
		auto drives = GetOpticalDriveList();
		if (!drives.empty())
			drive = drives.front();
	}
	if (!drive.empty())
		printf(" * CDVD: Opening drive '%s'...\n", drive.c_str());
}
