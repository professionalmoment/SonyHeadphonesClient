#include "WindowsBluetoothConnector.h"

void WSAStartupWrapper()
{
	int iResult;
	WSADATA wsaData;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		throw std::runtime_error("WSAStartup failed: " + std::to_string(iResult));
	}
}

WindowsBluetoothConnector::WindowsBluetoothConnector()
{
	static bool startedUp = false;
	if (!startedUp)
	{
		::WSAStartupWrapper();
		startedUp = true;
	}
	SOCKET sock = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	if (sock == INVALID_SOCKET)
	{
		throw std::runtime_error("Couldn't create socket: " + std::to_string(WSAGetLastError()));
	}

	ULONG enable = TRUE;
	if (::setsockopt(sock, SOL_RFCOMM, SO_BTH_AUTHENTICATE, reinterpret_cast<char*>(&enable), sizeof(enable)))
	{
		throw std::runtime_error("Couldn't create set SO_BTH_AUTHENTICATE: " + std::to_string(WSAGetLastError()));
	}

	if (::setsockopt(sock, SOL_RFCOMM, SO_BTH_ENCRYPT, reinterpret_cast<char*>(&enable), sizeof(enable)))
	{
		throw std::runtime_error("Couldn't create set SO_BTH_ENCRYPT: " + std::to_string(WSAGetLastError()));
	}
	WindowsBluetoothConnector::_initSocket();
}

void WindowsBluetoothConnector::connect(const std::string& addrStr)
{
	SOCKADDR_BTH sab = { 0 };
	sab.addressFamily = AF_BTH;
	RPC_STATUS errCode = ::UuidFromStringA((RPC_CSTR)XM3_UUID, &sab.serviceClassId);
	if (errCode != RPC_S_OK)
	{
		throw std::runtime_error("Couldn't create GUID: " + std::to_string(errCode));
	}
	sab.btAddr = MACStringToLong(addrStr);

	if (::connect(this->_socket, (sockaddr*)&sab, sizeof(sab)))
	{
		throw std::runtime_error("Couldn't connect: " + std::to_string(WSAGetLastError()));
	}
}

WindowsBluetoothConnector::~WindowsBluetoothConnector()
{
	if (this->_socket != INVALID_SOCKET)
	{
		::closesocket(this->_socket);
	}
}

int WindowsBluetoothConnector::send(char* buf, size_t length)
{
	auto bytesSent = ::send(this->_socket, buf, length, 0);
	if (bytesSent == SOCKET_ERROR)
	{
		throw std::runtime_error("Couldn't send: " + std::to_string(WSAGetLastError()));
	}
	return bytesSent;
}

std::vector<BluetoothDevice> WindowsBluetoothConnector::getConnectedDevices()
{
	std::vector<BluetoothDevice> res;
	std::vector<BluetoothDevice> devs_in_radio;

	HANDLE radio = NULL;
	BLUETOOTH_FIND_RADIO_PARAMS radio_search_params = { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) };
	HBLUETOOTH_RADIO_FIND radio_find_handle = NULL;

	// Search only for connected devices
	BLUETOOTH_DEVICE_SEARCH_PARAMS dev_search_params = {
	  sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS), 0, 0, 0, 1, 0, 15, NULL
	};

	// Iterate for available bluetooth radio devices
	radio_find_handle = BluetoothFindFirstRadio(&radio_search_params, &radio);
	if (!radio_find_handle)
	{
		throw std::runtime_error("BluetoothFindFirstRadio() failed with error code " + std::to_string(GetLastError()));
	}

	do {
		dev_search_params.hRadio = radio;
		devs_in_radio = findDevicesInRadio(&dev_search_params);
		res.insert(res.end(), devs_in_radio.begin(), devs_in_radio.end());
	} while (BluetoothFindNextRadio(radio_find_handle, &radio));

	// No more radio, close the radio handle
	if (!BluetoothFindRadioClose(radio_find_handle))
		throw std::runtime_error("BluetoothFindRadioClose(bt) failed with error code " + std::to_string(GetLastError()));

	return res;
}

void WindowsBluetoothConnector::disconnect() noexcept(false)
{
	if (this->_socket != INVALID_SOCKET)
	{

		if (::shutdown(this->_socket, SD_BOTH))
		{
			throw std::runtime_error("Couldn't shutdown connection: " + std::to_string(WSAGetLastError()));
		}

		if (::closesocket(this->_socket))
		{
			throw std::runtime_error("Couldn't disconnect: " + std::to_string(WSAGetLastError()));
		}

		this->_socket = INVALID_SOCKET;
		WindowsBluetoothConnector::_initSocket();
	}
	else
	{
		throw std::runtime_error("The socket was already closed, or it was never open");
	}
}

void WindowsBluetoothConnector::_initSocket()
{
	SOCKET sock = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	if (sock == INVALID_SOCKET)
	{
		throw std::runtime_error("Couldn't create socket: " + std::to_string(WSAGetLastError()));
	}

	ULONG enable = TRUE;
	if (::setsockopt(sock, SOL_RFCOMM, SO_BTH_AUTHENTICATE, reinterpret_cast<char*>(&enable), sizeof(enable)))
	{
		throw std::runtime_error("Couldn't set SO_BTH_AUTHENTICATE: " + std::to_string(WSAGetLastError()));
	}

	if (::setsockopt(sock, SOL_RFCOMM, SO_BTH_ENCRYPT, reinterpret_cast<char*>(&enable), sizeof(enable)))
	{
		throw std::runtime_error("Couldn't set SO_BTH_ENCRYPT: " + std::to_string(WSAGetLastError()));
	}

	this->_socket = sock;
}
std::string wstringToUtf8(const std::wstring& wstr)
{
	std::string strTo;
	const int num_chars = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wstr.length(), NULL, 0, NULL, NULL);

	if (num_chars > 0)
	{
		strTo.resize(num_chars);
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wstr.length(), &strTo[0], num_chars, NULL, NULL);
	}
	return strTo;
}

std::vector<BluetoothDevice> WindowsBluetoothConnector::findDevicesInRadio(BLUETOOTH_DEVICE_SEARCH_PARAMS* search_params)
{
	std::vector<BluetoothDevice> res;

	BLUETOOTH_DEVICE_INFO device_info = { sizeof(BLUETOOTH_DEVICE_INFO),0, };
	HBLUETOOTH_DEVICE_FIND dev_find_handle = NULL;

	// For each radio, get the first device
	dev_find_handle = BluetoothFindFirstDevice(search_params, &device_info);
	//TODO: This fails if there aren't any devices, check the conditions and return an empty vector
	if (!dev_find_handle)
	{
		throw std::runtime_error("BluetoothFindFirstDevice() failed with error code: " + std::to_string(GetLastError()));
	}

	// Get the device info
	do {
		res.emplace_back(BluetoothDevice{ wstringToUtf8(device_info.szName), MACBytesToString(device_info.Address.rgBytes) });
	} while (BluetoothFindNextDevice(dev_find_handle, &device_info));

	// NO more device, close the device handle
	if (!BluetoothFindDeviceClose(dev_find_handle))
		throw std::runtime_error("\nBluetoothFindDeviceClose(bt_dev) failed with error code: " + std::to_string(GetLastError()));

	return res;
}
