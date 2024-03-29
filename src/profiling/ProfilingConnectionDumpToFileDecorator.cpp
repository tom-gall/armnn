//
// Copyright © 2019 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "ProfilingConnectionDumpToFileDecorator.hpp"

#include <armnn/Exceptions.hpp>

#include <fstream>

#include <boost/numeric/conversion/cast.hpp>

namespace armnn
{

namespace profiling
{

ProfilingConnectionDumpToFileDecorator::ProfilingConnectionDumpToFileDecorator(
    std::unique_ptr<IProfilingConnection> connection,
    const Settings& settings)
    : m_Connection(std::move(connection))
    , m_Settings(settings)
{
    if (!m_Connection)
    {
        throw InvalidArgumentException("Connection cannot be nullptr");
    }
}

ProfilingConnectionDumpToFileDecorator::~ProfilingConnectionDumpToFileDecorator()
{
    Close();
}

bool ProfilingConnectionDumpToFileDecorator::IsOpen() const
{
    return m_Connection->IsOpen();
}

void ProfilingConnectionDumpToFileDecorator::Close()
{
    m_IncomingDumpFileStream.close();
    m_OutgoingDumpFileStream.close();
    m_Connection->Close();
}

bool ProfilingConnectionDumpToFileDecorator::WritePacket(const unsigned char* buffer, uint32_t length)
{
    bool success = true;
    if (m_Settings.m_DumpOutgoing)
    {
        success &= DumpOutgoingToFile(buffer, length);
    }
    success &= m_Connection->WritePacket(buffer, length);
    return success;
}

Packet ProfilingConnectionDumpToFileDecorator::ReadPacket(uint32_t timeout)
{
    Packet packet = m_Connection->ReadPacket(timeout);
    if (m_Settings.m_DumpIncoming)
    {
        DumpIncomingToFile(packet);
    }
    return packet;
}

bool ProfilingConnectionDumpToFileDecorator::OpenIncomingDumpFile()
{
    m_IncomingDumpFileStream.open(m_Settings.m_IncomingDumpFileName, std::ios::out | std::ios::binary);
    return m_IncomingDumpFileStream.is_open();
}

bool ProfilingConnectionDumpToFileDecorator::OpenOutgoingDumpFile()
{
    m_OutgoingDumpFileStream.open(m_Settings.m_OutgoingDumpFileName, std::ios::out | std::ios::binary);
    return m_OutgoingDumpFileStream.is_open();
}


/// Dumps incoming data into the file specified by m_Settings.m_IncomingDumpFileName.
/// If m_IgnoreFileErrors is set to true in m_Settings, write errors will be ignored,
/// i.e. the method will not throw an exception if it encounters an error while trying
/// to write the data into the specified file.
/// @param packet data packet to write
/// @return nothing
void ProfilingConnectionDumpToFileDecorator::DumpIncomingToFile(const Packet& packet)
{
    bool success = true;
    if (!m_IncomingDumpFileStream.is_open())
    {
        // attempt to open dump file
        success &= OpenIncomingDumpFile();
        if (!(success || m_Settings.m_IgnoreFileErrors))
        {
            Fail("Failed to open \"" + m_Settings.m_IncomingDumpFileName + "\" for writing");
        }
    }

    // attempt to write binary data from packet
    const unsigned int header       = packet.GetHeader();
    const unsigned int packetLength = packet.GetLength();

    m_IncomingDumpFileStream.write(reinterpret_cast<const char*>(&header), sizeof header);
    m_IncomingDumpFileStream.write(reinterpret_cast<const char*>(&packetLength), sizeof packetLength);
    m_IncomingDumpFileStream.write(reinterpret_cast<const char*>(packet.GetData()),
                                   boost::numeric_cast<std::streamsize>(packetLength));

    success &= m_IncomingDumpFileStream.good();
    if (!(success || m_Settings.m_IgnoreFileErrors))
    {
        Fail("Error writing incoming packet of " + std::to_string(packetLength) + " bytes");
    }
}

/// Dumps outgoing data into the file specified by m_Settings.m_OutgoingDumpFileName.
/// If m_IgnoreFileErrors is set to true in m_Settings, write errors will be ignored,
/// i.e. the method will not throw an exception if it encounters an error while trying
/// to write the data into the specified file. However, the return value will still
/// signal if the write has not been completed succesfully.
/// @param buffer pointer to data to write
/// @param length number of bytes to write
/// @return true if write successful, false otherwise
bool ProfilingConnectionDumpToFileDecorator::DumpOutgoingToFile(const unsigned char* buffer, uint32_t length)
{
    bool success = true;
    if (!m_OutgoingDumpFileStream.is_open())
    {
        // attempt to open dump file
        success &= OpenOutgoingDumpFile();
        if (!(success || m_Settings.m_IgnoreFileErrors))
        {
            Fail("Failed to open \"" + m_Settings.m_OutgoingDumpFileName + "\" for writing");
        }
    }

    // attempt to write binary data
    m_OutgoingDumpFileStream.write(reinterpret_cast<const char*>(buffer),
                                   boost::numeric_cast<std::streamsize>(length));
    success &= m_OutgoingDumpFileStream.good();
    if (!(success || m_Settings.m_IgnoreFileErrors))
    {
        Fail("Error writing outgoing packet of " + std::to_string(length) + " bytes");
    }

    return success;
}

void ProfilingConnectionDumpToFileDecorator::Fail(const std::string& errorMessage)
{
    Close();
    throw RuntimeException(errorMessage);
}

} // namespace profiling

} // namespace armnn
