#include "BluetoothWrapper.h"

BluetoothWrapper::BluetoothWrapper(std::unique_ptr<IBluetoothConnector> connector)
{
	this->_connector.swap(connector);
}

int BluetoothWrapper::sendCommand(const std::vector<char>& bytes)
{
	auto data = CommandSerializer::_packageDataForBt(bytes, DATA_TYPE::DATA_MDR, this->_seqNumber++);
	return this->_connector->send(data.data(), data.size());
}

void BluetoothWrapper::connect(const std::string& addr)
{
	this->_connector->connect(addr);
}

void BluetoothWrapper::disconnect()
{
	this->_seqNumber = 0;
	this->_connector->disconnect();
}


std::vector<BluetoothDevice> BluetoothWrapper::getConnectedDevices()
{
	return this->_connector->getConnectedDevices();
}