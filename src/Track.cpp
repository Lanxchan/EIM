#include "Track.h"
#include "ProcessorBase.h"
#include "MasterTrack.h"
#include "Main.h"
#include "websocket/Packets.h"

Track::Track(std::string name, std::string color, MasterTrack* masterTrack): AudioProcessorGraph(), name(name), color(color), masterTrack(masterTrack) {
	setChannelLayoutOfBus(true, 0, juce::AudioChannelSet::canonicalChannelSet(2));
	setChannelLayoutOfBus(false, 0, juce::AudioChannelSet::canonicalChannelSet(2));
	auto input = addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
	begin = addNode(std::make_unique<ProcessorBase>());
	end = addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
	midiIn = addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode))->nodeID;
	auto midiOut = addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::midiOutputNode))->nodeID;

	addConnection({ { input->nodeID, 0 }, { begin->nodeID, 0 } });
	addConnection({ { input->nodeID, 1 }, { begin->nodeID, 1 } });
	addConnection({ { begin->nodeID, 0 }, { end->nodeID, 0 } });
	addConnection({ { begin->nodeID, 1 }, { end->nodeID, 1 } });
	addConnection({ { midiIn, juce::AudioProcessorGraph::midiChannelIndex }, { midiOut, juce::AudioProcessorGraph::midiChannelIndex } });
	chain.get<1>().setGainLinear(1);
}

void Track::setGenerator(std::unique_ptr<PluginWrapper> instance) {
	auto node = addNode(std::move(instance));
	addConnection({ { node->nodeID, 0 }, { end->nodeID, 0 } });
	addConnection({ { node->nodeID, 1 }, { end->nodeID, 1 } });
	addConnection({ { midiIn, juce::AudioProcessorGraph::midiChannelIndex }, { node->nodeID, juce::AudioProcessorGraph::midiChannelIndex } });
}

void Track::setRateAndBufferSizeDetails(double newSampleRate, int newBlockSize) {
	AudioProcessorGraph::setRateAndBufferSizeDetails(newSampleRate, newBlockSize);
	for (auto it : getNodes()) it->getProcessor()->prepareToPlay(newSampleRate, newBlockSize);
	messageCollector.reset(newSampleRate);
}

void Track::addMidiEventsToBuffer(int sampleCount, juce::MidiBuffer& midiMessages) {
	auto& info = masterTrack->currentPositionInfo;
	if (info.isPlaying) {
		auto startTime = info.ppqPosition;
		auto totalTime = sampleCount / getSampleRate() / 60.0 * info.bpm * masterTrack->ppq;
		auto endTime = startTime + totalTime;
		double curTime;
		for (auto it = midiSequence.begin() + midiSequence.getNextIndexAtTime(startTime);
			it < midiSequence.end() && (curTime = (*it)->message.getTimeStamp()) < endTime; it++) {
			midiMessages.addEvent((*it)->message, juce::roundToInt((curTime - startTime) / totalTime * sampleCount));
		}
	}
	messageCollector.removeNextBlockOfMessages(midiMessages, sampleCount);
}

void Track::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
	addMidiEventsToBuffer(buffer.getNumSamples(), midiMessages);
	AudioProcessorGraph::processBlock(buffer, midiMessages);
	auto inoutBlock = juce::dsp::AudioBlock<float>(buffer).getSubsetChannelBlock(0, (size_t)2);
	chain.process(juce::dsp::ProcessContextReplacing<float>(inoutBlock));
}

void Track::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages) {
	addMidiEventsToBuffer(buffer.getNumSamples(), midiMessages);
	AudioProcessorGraph::processBlock(buffer, midiMessages);
}

void Track::addMidiEvents(juce::MidiMessageSequence seq, int timeFormat) {
	for (auto node : seq) {
		if (!node->message.isNoteOnOrOff()) continue;
		juce::MidiMessage msg = node->message;
		msg.setTimeStamp(juce::roundToInt(msg.getTimeStamp() / timeFormat * masterTrack->ppq));
		midiSequence.addEvent(msg, 0);
	}
	auto buf = makeTrackMidiDataPacket(1);
	writeMidiData(buf.get());
	EIMApplication::getEIMInstance()->listener->state->send(buf);
}

void Track::writeTrackInfo(ByteBuffer* buf) {
	buf->writeString(name);
	buf->writeString(color);
	buf->writeFloat(chain.get<1>().getGainLinear());
	buf->writeBoolean(currentNode->isBypassed());
	buf->writeBoolean(false);
}

void Track::writeMidiData(ByteBuffer* buf) {
	buf->writeString(uuid.toString());
	std::vector<std::tuple<juce::uint8, juce::uint8, juce::uint32, juce::uint32>> arr;
	midiSequence.updateMatchedPairs();
	for (auto& it : midiSequence) {
		if (!it->message.isNoteOn() || !it->noteOffObject) continue;
		arr.emplace_back(std::make_tuple(it->message.getNoteNumber(), it->message.getVelocity(), (juce::uint32) it->message.getTimeStamp(),
			(juce::uint32)(it->noteOffObject->message.getTimeStamp() - it->message.getTimeStamp())));
	}
	buf->writeUInt16((unsigned char) arr.size());
	juce::uint8 key, vel;
	juce::uint32 on, off;
	for (auto& it : arr) {
		std::tie(key, vel, on, off) = it;
		buf->writeUInt8(key);
		buf->writeUInt8(vel);
		buf->writeUInt32(on);
		buf->writeUInt32(off);
	}
}

void Track::setProcessingPrecision(ProcessingPrecision newPrecision) {
	AudioProcessorGraph::setProcessingPrecision(newPrecision);
	for (auto it : getNodes()) it->getProcessor()->setProcessingPrecision(newPrecision);
}

void Track::prepareToPlay(double sampleRate, int estimatedSamplesPerBlock) {
	AudioProcessorGraph::prepareToPlay(sampleRate, estimatedSamplesPerBlock);
	chain.prepare({ sampleRate, (juce::uint32)estimatedSamplesPerBlock, 2 });
	for (auto it : getNodes()) it->getProcessor()->prepareToPlay(sampleRate, estimatedSamplesPerBlock);
}

void Track::setPlayHead(juce::AudioPlayHead* newPlayHead) {
	AudioProcessorGraph::setPlayHead(newPlayHead);
	for (auto it : getNodes()) it->getProcessor()->setPlayHead(newPlayHead);
}

void Track::setMuted(bool val) {
	if (!currentNode) return;
	auto msg = juce::MidiMessage::allNotesOff(1);
	msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
	messageCollector.addMessageToQueue(msg);
	currentNode->setBypassed(val);
}
