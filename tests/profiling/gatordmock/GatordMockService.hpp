//
// Copyright © 2019 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <string>
#include <thread>
#include <atomic>

#include "../../src/profiling/CommandHandlerRegistry.hpp"
#include "../../src/profiling/Packet.hpp"

namespace armnn
{

namespace gatordmock
{

enum class TargetEndianness
{
    BeWire, LeWire
};

enum class PacketDirection
{
    Sending, Received
};

///  A class that implements a Mock Gatord server. It will listen on a specified Unix domain socket (UDS)
///  namespace for client connections. It will then allow opertaions to manage coutners while receiving counter data.
class GatordMockService
{
public:


    /// @param registry reference to a command handler registry.
    /// @param echoPackets if true the raw packets will be printed to stdout.
    GatordMockService(armnn::profiling::CommandHandlerRegistry& registry, bool echoPackets)
        : m_HandlerRegistry(registry)
        , m_EchoPackets(echoPackets)
    {
        m_PacketsReceivedCount.store(0, std::memory_order_relaxed);
    }

    ~GatordMockService()
    {
        // We have set SOCK_CLOEXEC on these sockets but we'll close them to be good citizens.
        close(m_ClientConnection);
        close(m_ListeningSocket);
    }

    /// Establish the Unix domain socket and set it to listen for connections.
    /// @param udsNamespace the namespace (socket address) associated with the listener.
    /// @return true only if the socket has been correctly setup.
    bool OpenListeningSocket(std::string udsNamespace);

    /// Block waiting to accept one client to connect to the UDS.
    /// @return the file descriptor of the client connection.
    int BlockForOneClient();

    /// Once the connection is open wait to receive the stream meta data packet from the client. Reading this
    /// packet differs from others as we need to determine endianness.
    /// @return true only if a valid stream met data packet has been received.
    bool WaitForStreamMetaData();

    /// Send a connection acknowledged packet back to the client.
    void SendConnectionAck();

    /// Start the thread that will receive all packets and print them nicely to stdout.
    bool LaunchReceivingThread();

    /// Return the total number of periodic counter capture packets received since the receive thread started.
    /// @return number of periodic counter capture packets received.
    uint32_t GetPacketsReceivedCount()
    {
        return m_PacketsReceivedCount.load(std::memory_order_acquire);
    }

    /// This is a placeholder method to prevent main exiting. It can be removed once the
    /// command handling code is added.
    void WaitForReceivingThread();

    /// Send the counter list to ArmNN.
    void SendPeriodicCounterSelectionList(uint period, std::vector<uint16_t> counters);

    /// Execute the WAIT command from the comamnd file.
    void WaitCommand(uint timeout);

    uint32_t GetStreamMetadataVersion()
    {
        return m_StreamMetaDataVersion;
    }

    uint32_t GetStreamMetadataMaxDataLen()
    {
        return m_StreamMetaDataMaxDataLen;
    }

    uint32_t GetStreamMetadataPid()
    {
        return m_StreamMetaDataPid;
    }

private:

    void ReceiveLoop(GatordMockService& mockService);

    /// Block on the client connection until a complete packet has been received. This is a placeholder function to
    /// enable early testing of the tool.
    /// @return true if a valid packet has been received.
    armnn::profiling::Packet WaitForPacket(uint32_t timeoutMs);

    armnn::profiling::Packet ReceivePacket();

    bool SendPacket(uint32_t packetFamily, uint32_t packetId, const u_char* data, uint32_t dataLength);

    void EchoPacket(PacketDirection direction, u_char* packet, size_t lengthInBytes);

    bool ReadHeader(uint32_t headerAsWords[2]);

    bool ReadFromSocket(u_char* packetData, uint32_t expectedLength);

    uint32_t ToUint32(u_char* data, TargetEndianness endianness);

    void InsertU32(uint32_t value, u_char* data, TargetEndianness endianness);

    static const uint32_t PIPE_MAGIC = 0x45495434;

    std::atomic<uint32_t> m_PacketsReceivedCount;
    TargetEndianness      m_Endianness;
    uint32_t              m_StreamMetaDataVersion;
    uint32_t              m_StreamMetaDataMaxDataLen;
    uint32_t              m_StreamMetaDataPid;

    armnn::profiling::CommandHandlerRegistry& m_HandlerRegistry;

    bool m_EchoPackets;
    int m_ListeningSocket;
    int m_ClientConnection;
    std::thread m_ListeningThread;
    std::atomic<bool> m_CloseReceivingThread;
};
} // namespace gatordmock

} // namespace armnn


