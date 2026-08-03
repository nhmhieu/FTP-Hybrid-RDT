#include "common/ftp_api.h"
#include "data/UDPReceiver.h"
#include "data/UDPSender.h"

namespace UDPData {
	bool sendFile(const std::string& filePath, const std::string& destIP, int destPort) {
		UDPSender sender;
		return sender.sendFile(filePath, destIP, destPort);
	}

	bool receiveFile(const std::string& savePath, int listenPort) {
		UDPReceiver receiver;
		return receiver.receiveFile(savePath, listenPort);
	}
}