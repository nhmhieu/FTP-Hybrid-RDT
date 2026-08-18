#include "common/ftp_api.h"
#include "data/UDPReceiver.h"
#include "data/UDPSender.h"

namespace UDPData {
	bool sendFile(const std::string& filePath, const std::string& destIP, int destPort,
		const std::atomic<bool>* cancel) {
		UDPSender sender;
		return sender.sendFile(filePath, destIP, destPort, cancel);
	}

	bool receiveFile(const std::string& savePath, int listenPort) {
		UDPReceiver receiver;
		return receiver.receiveFile(savePath, listenPort);
	}

	bool receiveFile(const std::string& savePath, int listenPort, std::atomic<int>& readyState, const std::atomic<bool>* cancel) {
		UDPReceiver receiver;
		return receiver.receiveFile(savePath, listenPort, &readyState, cancel);
	}
	bool receiveFile(const std::string& savePath, int listenPort, const std::atomic<bool>* cancel) {
		UDPReceiver receiver;
		return receiver.receiveFile(savePath, listenPort, nullptr, cancel);
	}
	bool receivePassiveFile(const std::string& savePath, int listenPort,
		const std::string& serverIP, int serverPort, std::atomic<int>& readyState, const std::atomic<bool>* cancel) {
		UDPReceiver receiver;
		return receiver.receivePassiveFile(savePath, listenPort, serverIP, serverPort, &readyState, cancel);
	}
}
