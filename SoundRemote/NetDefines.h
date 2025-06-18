#pragma once

#include <stdint.h>

#include <boost/asio/ip/address.hpp>

namespace Net {
	namespace Packet {
		// Header fields
		using SignatureType = uint16_t;
		using CategoryType = uint8_t;
		using SizeType = uint16_t;
		// Data fields
		using ProtocolVersionType = uint8_t;
		using RequestIdType = uint16_t;
		using CompressionType = uint8_t;
		using KeyType = uint8_t;
		using ModsType = uint8_t;
		using SequenceNumberType = uint32_t;
		constexpr int ackCustomDataSize = 4;
		// Header data
		constexpr int headerSize = sizeof(SignatureType) + sizeof(CategoryType) + sizeof(SizeType);
		constexpr int signatureOffset = 0;
		constexpr int categoryOffset = sizeof(SignatureType);
		constexpr int sizeOffset = sizeof(SignatureType) + sizeof(CategoryType);
		constexpr int dataOffset = headerSize;
		// Packet data
		constexpr int keystrokeSize = sizeof(KeyType) + sizeof(ModsType);
		constexpr int ackSize = sizeof(RequestIdType) + ackCustomDataSize;
		constexpr int ackCustomDataOffset = dataOffset + sizeof(RequestIdType);
		constexpr int sequenceNumberSize = sizeof(SequenceNumberType);
		constexpr int audioDataOffset = dataOffset + sequenceNumberSize;
		struct ConnectData {
			ProtocolVersionType protocol;
			RequestIdType requestId;
			CompressionType compression;
			static const int size = sizeof(ProtocolVersionType) + sizeof(RequestIdType) + sizeof(CompressionType);
		};
		struct SetFormatData {
			RequestIdType requestId;
			CompressionType compression;
			static const int size = sizeof(RequestIdType) + sizeof(CompressionType);
		};

		constexpr SignatureType protocolSignature = 0xA571u;

		enum class Category: CategoryType {
			Error = 0,
			Connect = 0x01u,
			Disconnect = 0x02u,
			SetFormat = 0x03u,
			Keystroke = 0x10u,
			AudioDataUncompressed = 0x20u,
			AudioDataOpus = 0x21u,
			ClientKeepAlive = 0x30u,
			ServerKeepAlive = 0x31u,
			Ack = 0xF0u
		};
	}
	constexpr Packet::ProtocolVersionType protocolVersion = 1u;

	using Address = boost::asio::ip::address;

	constexpr uint16_t defaultServerPort = 15711u;
	constexpr uint16_t defaultClientPort = 15712u;

	constexpr int inputPacketSize = 1024;
}
