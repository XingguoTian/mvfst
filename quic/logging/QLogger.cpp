/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#include <quic/logging/QLogger.h>

#include <quic/logging/QLoggerConstants.h>

namespace quic {

void QLogger::add(const RegularQuicPacket& regularPacket, uint64_t packetSize) {
  auto event = std::make_unique<QLogPacketEvent>();

  event->packetNum = folly::variant_match(
      regularPacket.header,
      [](const auto& h) { return h.getPacketSequenceNum(); });
  event->packetSize = packetSize;
  event->eventType = EventType::PacketReceived;
  event->packetType = folly::variant_match(
      regularPacket.header,
      [](const LongHeader& header) { return toString(header.getHeaderType()); },
      [](const ShortHeader& /* unused*/) {
        return kShortHeaderPacketType.toString();
      });

  // looping through the packet to store logs created from frames in the packet
  for (const auto& quicFrame : regularPacket.frames) {
    folly::variant_match(
        quicFrame,
        [&](const PaddingFrame& /* unused */) {
          event->frames.push_back(std::make_unique<PaddingFrameLog>());
        },
        [&](const RstStreamFrame& frame) {
          event->frames.push_back(std::make_unique<RstStreamFrameLog>(
              frame.streamId, frame.errorCode, frame.offset));
        },
        [&](const ConnectionCloseFrame& frame) {
          event->frames.push_back(std::make_unique<ConnectionCloseFrameLog>(
              frame.errorCode, frame.reasonPhrase, frame.closingFrameType));
        },
        [&](const ApplicationCloseFrame& frame) {
          event->frames.push_back(std::make_unique<ApplicationCloseFrameLog>(
              frame.errorCode, frame.reasonPhrase));
        },
        [&](const MaxDataFrame& frame) {
          event->frames.push_back(
              std::make_unique<MaxDataFrameLog>(frame.maximumData));
        },
        [&](const MaxStreamDataFrame& frame) {
          event->frames.push_back(std::make_unique<MaxStreamDataFrameLog>(
              frame.streamId, frame.maximumData));
        },
        [&](const MaxStreamsFrame& frame) {
          event->frames.push_back(std::make_unique<MaxStreamsFrameLog>(
              frame.maxStreams, frame.isForBidirectional));
        },
        [&](const StreamsBlockedFrame& frame) {
          event->frames.push_back(std::make_unique<StreamsBlockedFrameLog>(
              frame.streamLimit, frame.isForBidirectional));
        },
        [&](const PingFrame& /* unused */) {
          event->frames.push_back(std::make_unique<PingFrameLog>());
        },
        [&](const DataBlockedFrame& frame) {
          event->frames.push_back(
              std::make_unique<DataBlockedFrameLog>(frame.dataLimit));
        },
        [&](const StreamDataBlockedFrame& frame) {
          event->frames.push_back(std::make_unique<StreamDataBlockedFrameLog>(
              frame.streamId, frame.dataLimit));
        },
        [&](const WriteAckFrame& frame) {
          event->frames.push_back(std::make_unique<AckFrameLog>(
              frame.ackBlocks.back().end, frame.ackDelay));
        },
        [&](const ReadAckFrame& frame) {
          event->frames.push_back(std::make_unique<AckFrameLog>(
              frame.largestAcked, frame.ackDelay));
        },
        [&](const WriteStreamFrame& frame) {
          event->frames.push_back(std::make_unique<StreamFrameLog>(
              frame.streamId, frame.offset, frame.len, frame.fin));
        },
        [&](const WriteCryptoFrame& frame) {
          event->frames.push_back(
              std::make_unique<CryptoFrameLog>(frame.offset, frame.len));
        },
        [&](const ReadStreamFrame& frame) {
          event->frames.push_back(std::make_unique<StreamFrameLog>(
              frame.streamId, frame.offset, frame.data->length(), frame.fin));
        },
        [&](const ReadCryptoFrame& frame) {
          event->frames.push_back(std::make_unique<CryptoFrameLog>(
              frame.offset, frame.data->length()));
        },
        [&](const ReadNewTokenFrame& /* unused */) {
          event->frames.push_back(std::make_unique<ReadNewTokenFrameLog>());
        },
        [&](const StopSendingFrame& frame) {
          event->frames.push_back(std::make_unique<StopSendingFrameLog>(
              frame.streamId, frame.errorCode));
        },
        [&](const MinStreamDataFrame& frame) {
          event->frames.push_back(std::make_unique<MinStreamDataFrameLog>(
              frame.streamId, frame.maximumData, frame.minimumStreamOffset));
        },
        [&](const ExpiredStreamDataFrame& frame) {
          event->frames.push_back(std::make_unique<ExpiredStreamDataFrameLog>(
              frame.streamId, frame.minimumStreamOffset));
        },
        [&](const PathChallengeFrame& frame) {
          event->frames.push_back(
              std::make_unique<PathChallengeFrameLog>(frame.pathData));
        },
        [&](const PathResponseFrame& frame) {
          event->frames.push_back(
              std::make_unique<PathResponseFrameLog>(frame.pathData));
        },
        [&](const NewConnectionIdFrame& frame) {
          event->frames.push_back(std::make_unique<NewConnectionIdFrameLog>(
              frame.sequence, frame.token));
        },
        [&](const auto& /* unused */) {
          // Ignore other frames.
        });
  }
  logs.push_back(std::move(event));
}

void QLogger::add(
    const RegularQuicWritePacket& writePacket,
    uint64_t packetSize) {
  auto event = std::make_unique<QLogPacketEvent>();

  event->packetNum = folly::variant_match(
      writePacket.header,
      [](const auto& h) { return h.getPacketSequenceNum(); });
  event->packetSize = packetSize;
  event->eventType = EventType::PacketSent;
  event->packetType = folly::variant_match(
      writePacket.header,
      [](const LongHeader& header) { return toString(header.getHeaderType()); },
      [](const ShortHeader& /* unused*/) {
        return kShortHeaderPacketType.toString();
      });

  // looping through the packet to store logs created from frames in the packet
  std::vector<std::unique_ptr<QLogFrame>> frames;
  for (const auto& quicFrame : writePacket.frames) {
    folly::variant_match(
        quicFrame,
        [&](const PaddingFrame& /* unused */) {
          event->frames.push_back(std::make_unique<PaddingFrameLog>());
        },
        [&](const RstStreamFrame& frame) {
          event->frames.push_back(std::make_unique<RstStreamFrameLog>(
              frame.streamId, frame.errorCode, frame.offset));
        },
        [&](const ConnectionCloseFrame& frame) {
          event->frames.push_back(std::make_unique<ConnectionCloseFrameLog>(
              frame.errorCode, frame.reasonPhrase, frame.closingFrameType));
        },
        [&](const ApplicationCloseFrame& frame) {
          event->frames.push_back(std::make_unique<ApplicationCloseFrameLog>(
              frame.errorCode, frame.reasonPhrase));
        },
        [&](const MaxDataFrame& frame) {
          event->frames.push_back(
              std::make_unique<MaxDataFrameLog>(frame.maximumData));
        },
        [&](const MaxStreamDataFrame& frame) {
          event->frames.push_back(std::make_unique<MaxStreamDataFrameLog>(
              frame.streamId, frame.maximumData));
        },
        [&](const MaxStreamsFrame& frame) {
          event->frames.push_back(std::make_unique<MaxStreamsFrameLog>(
              frame.maxStreams, frame.isForBidirectional));
        },
        [&](const StreamsBlockedFrame& frame) {
          event->frames.push_back(std::make_unique<StreamsBlockedFrameLog>(
              frame.streamLimit, frame.isForBidirectional));
        },
        [&](const PingFrame& /* unused */) {
          event->frames.push_back(std::make_unique<PingFrameLog>());
        },
        [&](const DataBlockedFrame& frame) {
          event->frames.push_back(
              std::make_unique<DataBlockedFrameLog>(frame.dataLimit));
        },
        [&](const StreamDataBlockedFrame& frame) {
          event->frames.push_back(std::make_unique<StreamDataBlockedFrameLog>(
              frame.streamId, frame.dataLimit));
        },
        [&](const WriteAckFrame& frame) {
          event->frames.push_back(std::make_unique<AckFrameLog>(
              frame.ackBlocks.back().end, frame.ackDelay));
        },
        [&](const ReadAckFrame& frame) {
          event->frames.push_back(std::make_unique<AckFrameLog>(
              frame.largestAcked, frame.ackDelay));
        },
        [&](const WriteStreamFrame& frame) {
          event->frames.push_back(std::make_unique<StreamFrameLog>(
              frame.streamId, frame.offset, frame.len, frame.fin));
        },
        [&](const WriteCryptoFrame& frame) {
          event->frames.push_back(
              std::make_unique<CryptoFrameLog>(frame.offset, frame.len));
        },
        [&](const ReadStreamFrame& frame) {
          event->frames.push_back(std::make_unique<StreamFrameLog>(
              frame.streamId, frame.offset, frame.data->length(), frame.fin));
        },
        [&](const ReadCryptoFrame& frame) {
          event->frames.push_back(std::make_unique<CryptoFrameLog>(
              frame.offset, frame.data->length()));
        },
        [&](const ReadNewTokenFrame& /* unused */) {
          event->frames.push_back(std::make_unique<ReadNewTokenFrameLog>());
        },
        [&](const StopSendingFrame& frame) {
          event->frames.push_back(std::make_unique<StopSendingFrameLog>(
              frame.streamId, frame.errorCode));
        },
        [&](const MinStreamDataFrame& frame) {
          event->frames.push_back(std::make_unique<MinStreamDataFrameLog>(
              frame.streamId, frame.maximumData, frame.minimumStreamOffset));
        },
        [&](const ExpiredStreamDataFrame& frame) {
          event->frames.push_back(std::make_unique<ExpiredStreamDataFrameLog>(
              frame.streamId, frame.minimumStreamOffset));
        },
        [&](const PathChallengeFrame& frame) {
          event->frames.push_back(
              std::make_unique<PathChallengeFrameLog>(frame.pathData));
        },
        [&](const PathResponseFrame& frame) {
          event->frames.push_back(
              std::make_unique<PathResponseFrameLog>(frame.pathData));
        },
        [&](const NewConnectionIdFrame& frame) {
          event->frames.push_back(std::make_unique<NewConnectionIdFrameLog>(
              frame.sequence, frame.token));
        },
        [&](const auto& /* unused */) {
          // Ignore other frames.
        });
  }
  logs.push_back(std::move(event));
}

void QLogger::add(
    const VersionNegotiationPacket& versionPacket,
    size_t packetSize,
    bool isPacketRecvd) {
  auto event = std::make_unique<QLogVersionNegotiationEvent>();
  event->packetSize = packetSize;
  event->eventType =
      isPacketRecvd ? EventType::PacketReceived : EventType::PacketSent;
  event->packetType = std::to_string(versionPacket.packetType);
  event->versionLog = std::make_unique<VersionNegotiationLog>(
      VersionNegotiationLog(versionPacket.versions));
  logs.push_back(std::move(event));
}

folly::dynamic QLogger::toDynamic() {
  return d;
}

} // namespace quic