#include "Main.h"
#include "websocket/Packets.h"
#include "websocket/WebSocketSession.h"

void EIMApplication::handlePacket(WebSocketSession* session) {
	auto &buf = session->buffer;
	switch (buf.readUInt8()) {
	case ServerboundPacket::ServerboundReply:
		break;
	case ServerboundPacket::ServerboundGetExplorerData: {
		auto replyId = buf.readUInt32();
		auto type = buf.readUInt8();
		auto path = buf.readString();
		auto out = makeReplyPacket(replyId);
		switch (type) {
		case 1:
			if (path.empty()) {
				std::unordered_set<juce::String> map;
				for (auto& it : mainWindow->masterTrack->knownPluginList.getTypes()) map.emplace(it.manufacturerName);
				out->writeUInt32((unsigned long)map.size());
				for (auto& it : map) out->writeString(it);
				out->writeUInt32(0);
			} else {
				out->writeUInt32(0);
				std::vector<juce::String> arr;
				for (auto& it : mainWindow->masterTrack->knownPluginList.getTypes()) {
					if (path == it.manufacturerName) arr.emplace_back(it.name + "#EIM#" + it.fileOrIdentifier);
				}
				out->writeUInt32((unsigned long)arr.size());
				for (auto& it : arr) out->writeString(it);
			}
			session->send(out);
		}
		break;
	}
	case ServerboundPacket::ServerboundCreateTrack: {
		auto replyId = buf.readInt32();
		auto name = buf.readString();
		auto color = buf.readString();
		buf.readUInt8();
		auto identifier = buf.readString();
		auto track = mainWindow->masterTrack->createTrack(name, color);
		if (!identifier.empty()) {
			auto type = mainWindow->masterTrack->knownPluginList.getTypeForFile(identifier);
			if (type != nullptr) {
				auto track0 = (Track*)track->getProcessor();
				track0->name = type->name.toStdString();
				mainWindow->masterTrack->loadPluginAsync(std::move(type), [track0, session, replyId](std::unique_ptr<PluginWrapper> instance, const std::string& err) {
					auto out = makeReplyPacket(replyId);
					if (err.empty()) track0->setGenerator(std::move(instance));
					out->writeString(err);
					session->send(out);
				});
			}
		}
		listener->syncTrackInfo();
		break;
	}
	case ServerboundPacket::ServerboundRefresh:
		listener->syncTrackInfo();
		break;
	case ServerboundPacket::ServerboundMidiMessage:
		auto id = buf.readUInt8();
		auto byte1 = buf.readUInt8();
		auto byte2 = buf.readUInt8();
		auto byte3 = buf.readUInt8();
		auto& tracks = mainWindow->masterTrack->tracks;
		DBG("" << id << " " << byte1 << " " << byte2 << " " << byte3);
		if (tracks.size() <= id) return;
		((Track*)tracks[id]->getProcessor())->getMidiMessageCollector()
			.addMessageToQueue(juce::MidiMessage(byte1, byte2, byte3, juce::Time::getMillisecondCounterHiRes() * 0.001));
	}
}