//
// Copyright © 2019 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "SendCounterPacketTests.hpp"

#include <CounterDirectory.hpp>
#include <BufferManager.hpp>
#include <EncodeVersion.hpp>
#include <ProfilingUtils.hpp>
#include <SendCounterPacket.hpp>

#include <armnn/Exceptions.hpp>
#include <armnn/Conversion.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include <chrono>

using namespace armnn::profiling;

namespace
{

void SetNotConnectedProfilingState(ProfilingStateMachine& profilingStateMachine)
{
    ProfilingState currentState = profilingStateMachine.GetCurrentState();
    switch (currentState)
    {
    case ProfilingState::WaitingForAck:
        profilingStateMachine.TransitionToState(ProfilingState::Active);
    case ProfilingState::Uninitialised:
    case ProfilingState::Active:
        profilingStateMachine.TransitionToState(ProfilingState::NotConnected);
    case ProfilingState::NotConnected:
        return;
    default:
        BOOST_CHECK_MESSAGE(false, "Invalid profiling state");
    }
}

void SetWaitingForAckProfilingState(ProfilingStateMachine& profilingStateMachine)
{
    ProfilingState currentState = profilingStateMachine.GetCurrentState();
    switch (currentState)
    {
    case ProfilingState::Uninitialised:
    case ProfilingState::Active:
        profilingStateMachine.TransitionToState(ProfilingState::NotConnected);
    case ProfilingState::NotConnected:
        profilingStateMachine.TransitionToState(ProfilingState::WaitingForAck);
    case ProfilingState::WaitingForAck:
        return;
    default:
        BOOST_CHECK_MESSAGE(false, "Invalid profiling state");
    }
}

void SetActiveProfilingState(ProfilingStateMachine& profilingStateMachine)
{
    ProfilingState currentState = profilingStateMachine.GetCurrentState();
    switch (currentState)
    {
    case ProfilingState::Uninitialised:
        profilingStateMachine.TransitionToState(ProfilingState::NotConnected);
    case ProfilingState::NotConnected:
        profilingStateMachine.TransitionToState(ProfilingState::WaitingForAck);
    case ProfilingState::WaitingForAck:
        profilingStateMachine.TransitionToState(ProfilingState::Active);
    case ProfilingState::Active:
        return;
    default:
        BOOST_CHECK_MESSAGE(false, "Invalid profiling state");
    }
}

} // Anonymous namespace

BOOST_AUTO_TEST_SUITE(SendCounterPacketTests)

BOOST_AUTO_TEST_CASE(MockSendCounterPacketTest)
{
    MockBufferManager mockBuffer(512);
    MockSendCounterPacket mockSendCounterPacket(mockBuffer);

    mockSendCounterPacket.SendStreamMetaDataPacket();

    auto packetBuffer = mockBuffer.GetReadableBuffer();
    const char* buffer = reinterpret_cast<const char*>(packetBuffer->GetReadableData());

    BOOST_TEST(strcmp(buffer, "SendStreamMetaDataPacket") == 0);

    mockBuffer.MarkRead(packetBuffer);

    CounterDirectory counterDirectory;
    mockSendCounterPacket.SendCounterDirectoryPacket(counterDirectory);

    packetBuffer = mockBuffer.GetReadableBuffer();
    buffer = reinterpret_cast<const char*>(packetBuffer->GetReadableData());

    BOOST_TEST(strcmp(buffer, "SendCounterDirectoryPacket") == 0);

    mockBuffer.MarkRead(packetBuffer);

    uint64_t timestamp = 0;
    std::vector<std::pair<uint16_t, uint32_t>> indexValuePairs;

    mockSendCounterPacket.SendPeriodicCounterCapturePacket(timestamp, indexValuePairs);

    packetBuffer = mockBuffer.GetReadableBuffer();
    buffer = reinterpret_cast<const char*>(packetBuffer->GetReadableData());

    BOOST_TEST(strcmp(buffer, "SendPeriodicCounterCapturePacket") == 0);

    mockBuffer.MarkRead(packetBuffer);

    uint32_t capturePeriod = 0;
    std::vector<uint16_t> selectedCounterIds;
    mockSendCounterPacket.SendPeriodicCounterSelectionPacket(capturePeriod, selectedCounterIds);

    packetBuffer = mockBuffer.GetReadableBuffer();
    buffer = reinterpret_cast<const char*>(packetBuffer->GetReadableData());

    BOOST_TEST(strcmp(buffer, "SendPeriodicCounterSelectionPacket") == 0);

    mockBuffer.MarkRead(packetBuffer);
}

BOOST_AUTO_TEST_CASE(SendPeriodicCounterSelectionPacketTest)
{
    ProfilingStateMachine profilingStateMachine;

    // Error no space left in buffer
    MockBufferManager mockBuffer1(10);
    SendCounterPacket sendPacket1(profilingStateMachine, mockBuffer1);

    uint32_t capturePeriod = 1000;
    std::vector<uint16_t> selectedCounterIds;
    BOOST_CHECK_THROW(sendPacket1.SendPeriodicCounterSelectionPacket(capturePeriod, selectedCounterIds),
                      BufferExhaustion);

    // Packet without any counters
    MockBufferManager mockBuffer2(512);
    SendCounterPacket sendPacket2(profilingStateMachine, mockBuffer2);

    sendPacket2.SendPeriodicCounterSelectionPacket(capturePeriod, selectedCounterIds);
    auto readBuffer2 = mockBuffer2.GetReadableBuffer();

    uint32_t headerWord0 = ReadUint32(readBuffer2, 0);
    uint32_t headerWord1 = ReadUint32(readBuffer2, 4);
    uint32_t period = ReadUint32(readBuffer2, 8);

    BOOST_TEST(((headerWord0 >> 26) & 0x3F) == 0);  // packet family
    BOOST_TEST(((headerWord0 >> 16) & 0x3FF) == 4); // packet id
    BOOST_TEST(headerWord1 == 4);                   // data lenght
    BOOST_TEST(period == 1000);                     // capture period

    // Full packet message
    MockBufferManager mockBuffer3(512);
    SendCounterPacket sendPacket3(profilingStateMachine, mockBuffer3);

    selectedCounterIds.reserve(5);
    selectedCounterIds.emplace_back(100);
    selectedCounterIds.emplace_back(200);
    selectedCounterIds.emplace_back(300);
    selectedCounterIds.emplace_back(400);
    selectedCounterIds.emplace_back(500);
    sendPacket3.SendPeriodicCounterSelectionPacket(capturePeriod, selectedCounterIds);
    auto readBuffer3 = mockBuffer3.GetReadableBuffer();

    headerWord0 = ReadUint32(readBuffer3, 0);
    headerWord1 = ReadUint32(readBuffer3, 4);
    period = ReadUint32(readBuffer3, 8);

    BOOST_TEST(((headerWord0 >> 26) & 0x3F) == 0);  // packet family
    BOOST_TEST(((headerWord0 >> 16) & 0x3FF) == 4); // packet id
    BOOST_TEST(headerWord1 == 14);                  // data lenght
    BOOST_TEST(period == 1000);                     // capture period

    uint16_t counterId = 0;
    uint32_t offset = 12;

    // Counter Ids
    for(const uint16_t& id : selectedCounterIds)
    {
        counterId = ReadUint16(readBuffer3, offset);
        BOOST_TEST(counterId == id);
        offset += 2;
    }
}

BOOST_AUTO_TEST_CASE(SendPeriodicCounterCapturePacketTest)
{
    ProfilingStateMachine profilingStateMachine;

    // Error no space left in buffer
    MockBufferManager mockBuffer1(10);
    SendCounterPacket sendPacket1(profilingStateMachine, mockBuffer1);

    auto captureTimestamp = std::chrono::steady_clock::now();
    uint64_t time =  static_cast<uint64_t >(captureTimestamp.time_since_epoch().count());
    std::vector<std::pair<uint16_t, uint32_t>> indexValuePairs;

    BOOST_CHECK_THROW(sendPacket1.SendPeriodicCounterCapturePacket(time, indexValuePairs),
                      BufferExhaustion);

    // Packet without any counters
    MockBufferManager mockBuffer2(512);
    SendCounterPacket sendPacket2(profilingStateMachine, mockBuffer2);

    sendPacket2.SendPeriodicCounterCapturePacket(time, indexValuePairs);
    auto readBuffer2 = mockBuffer2.GetReadableBuffer();

    uint32_t headerWord0 = ReadUint32(readBuffer2, 0);
    uint32_t headerWord1 = ReadUint32(readBuffer2, 4);
    uint64_t readTimestamp = ReadUint64(readBuffer2, 8);

    BOOST_TEST(((headerWord0 >> 26) & 0x3F) == 1);   // packet family
    BOOST_TEST(((headerWord0 >> 19) & 0x3F) == 0);   // packet class
    BOOST_TEST(((headerWord0 >> 16) & 0x3) == 0);    // packet type
    BOOST_TEST(headerWord1 == 8);                    // data length
    BOOST_TEST(time == readTimestamp);               // capture period

    // Full packet message
    MockBufferManager mockBuffer3(512);
    SendCounterPacket sendPacket3(profilingStateMachine, mockBuffer3);

    indexValuePairs.reserve(5);
    indexValuePairs.emplace_back(std::make_pair<uint16_t, uint32_t >(0, 100));
    indexValuePairs.emplace_back(std::make_pair<uint16_t, uint32_t >(1, 200));
    indexValuePairs.emplace_back(std::make_pair<uint16_t, uint32_t >(2, 300));
    indexValuePairs.emplace_back(std::make_pair<uint16_t, uint32_t >(3, 400));
    indexValuePairs.emplace_back(std::make_pair<uint16_t, uint32_t >(4, 500));
    sendPacket3.SendPeriodicCounterCapturePacket(time, indexValuePairs);
    auto readBuffer3 = mockBuffer3.GetReadableBuffer();

    headerWord0 = ReadUint32(readBuffer3, 0);
    headerWord1 = ReadUint32(readBuffer3, 4);
    uint64_t readTimestamp2 = ReadUint64(readBuffer3, 8);

    BOOST_TEST(((headerWord0 >> 26) & 0x3F) == 1);   // packet family
    BOOST_TEST(((headerWord0 >> 19) & 0x3F) == 0);   // packet class
    BOOST_TEST(((headerWord0 >> 16) & 0x3) == 0);    // packet type
    BOOST_TEST(headerWord1 == 38);                   // data length
    BOOST_TEST(time == readTimestamp2);              // capture period

    uint16_t counterIndex = 0;
    uint32_t counterValue = 100;
    uint32_t offset = 16;

    // Counter Ids
    for (auto it = indexValuePairs.begin(), end = indexValuePairs.end(); it != end; ++it)
    {
        // Check Counter Index
        uint16_t readIndex = ReadUint16(readBuffer3, offset);
        BOOST_TEST(counterIndex == readIndex);
        counterIndex++;
        offset += 2;

        // Check Counter Value
        uint32_t readValue = ReadUint32(readBuffer3, offset);
        BOOST_TEST(counterValue == readValue);
        counterValue += 100;
        offset += 4;
    }

}

BOOST_AUTO_TEST_CASE(SendStreamMetaDataPacketTest)
{
    using boost::numeric_cast;

    uint32_t sizeUint32 = numeric_cast<uint32_t>(sizeof(uint32_t));

    ProfilingStateMachine profilingStateMachine;

    // Error no space left in buffer
    MockBufferManager mockBuffer1(10);
    SendCounterPacket sendPacket1(profilingStateMachine, mockBuffer1);
    BOOST_CHECK_THROW(sendPacket1.SendStreamMetaDataPacket(), armnn::profiling::BufferExhaustion);

    // Full metadata packet

    std::string processName = GetProcessName().substr(0, 60);

    uint32_t infoSize = numeric_cast<uint32_t>(GetSoftwareInfo().size()) > 0 ?
                        numeric_cast<uint32_t>(GetSoftwareInfo().size()) + 1 : 0;
    uint32_t hardwareVersionSize = numeric_cast<uint32_t>(GetHardwareVersion().size()) > 0 ?
                                   numeric_cast<uint32_t>(GetHardwareVersion().size()) + 1 : 0;
    uint32_t softwareVersionSize = numeric_cast<uint32_t>(GetSoftwareVersion().size()) > 0 ?
                                   numeric_cast<uint32_t>(GetSoftwareVersion().size()) + 1 : 0;
    uint32_t processNameSize = numeric_cast<uint32_t>(processName.size()) > 0 ?
                               numeric_cast<uint32_t>(processName.size()) + 1 : 0;

    uint32_t packetEntries = 6;

    MockBufferManager mockBuffer2(512);
    SendCounterPacket sendPacket2(profilingStateMachine, mockBuffer2);
    sendPacket2.SendStreamMetaDataPacket();
    auto readBuffer2 = mockBuffer2.GetReadableBuffer();

    uint32_t headerWord0 = ReadUint32(readBuffer2, 0);
    uint32_t headerWord1 = ReadUint32(readBuffer2, sizeUint32);

    BOOST_TEST(((headerWord0 >> 26) & 0x3F) == 0); // packet family
    BOOST_TEST(((headerWord0 >> 16) & 0x3FF) == 0); // packet id

    uint32_t totalLength = numeric_cast<uint32_t>(2 * sizeUint32 + 10 * sizeUint32 + infoSize + hardwareVersionSize +
                                                  softwareVersionSize + processNameSize + sizeUint32 +
                                                  2 * packetEntries * sizeUint32);

    BOOST_TEST(headerWord1 == totalLength - (2 * sizeUint32)); // data length

    uint32_t offset = sizeUint32 * 2;
    BOOST_TEST(ReadUint32(readBuffer2, offset) == SendCounterPacket::PIPE_MAGIC); // pipe_magic
    offset += sizeUint32;
    BOOST_TEST(ReadUint32(readBuffer2, offset) == EncodeVersion(1, 0, 0)); // stream_metadata_version
    offset += sizeUint32;
    BOOST_TEST(ReadUint32(readBuffer2, offset) == SendCounterPacket::MAX_METADATA_PACKET_LENGTH); // max_data_len
    offset += sizeUint32;
    BOOST_TEST(ReadUint32(readBuffer2, offset) == numeric_cast<uint32_t>(getpid())); // pid
    offset += sizeUint32;
    uint32_t poolOffset = 10 * sizeUint32;
    BOOST_TEST(ReadUint32(readBuffer2, offset) == (infoSize ? poolOffset : 0)); // offset_info
    offset += sizeUint32;
    poolOffset += infoSize;
    BOOST_TEST(ReadUint32(readBuffer2, offset) == (hardwareVersionSize ? poolOffset : 0)); // offset_hw_version
    offset += sizeUint32;
    poolOffset += hardwareVersionSize;
    BOOST_TEST(ReadUint32(readBuffer2, offset) == (softwareVersionSize ? poolOffset : 0)); // offset_sw_version
    offset += sizeUint32;
    poolOffset += softwareVersionSize;
    BOOST_TEST(ReadUint32(readBuffer2, offset) == (processNameSize ? poolOffset : 0)); // offset_process_name
    offset += sizeUint32;
    poolOffset += processNameSize;
    BOOST_TEST(ReadUint32(readBuffer2, offset) == (packetEntries ? poolOffset : 0)); // offset_packet_version_table
    offset += sizeUint32;
    BOOST_TEST(ReadUint32(readBuffer2, offset) == 0); // reserved

    const unsigned char* readData2 = readBuffer2->GetReadableData();

    offset += sizeUint32;
    if (infoSize)
    {
        BOOST_TEST(strcmp(reinterpret_cast<const char *>(&readData2[offset]), GetSoftwareInfo().c_str()) == 0);
        offset += infoSize;
    }

    if (hardwareVersionSize)
    {
        BOOST_TEST(strcmp(reinterpret_cast<const char *>(&readData2[offset]), GetHardwareVersion().c_str()) == 0);
        offset += hardwareVersionSize;
    }

    if (softwareVersionSize)
    {
        BOOST_TEST(strcmp(reinterpret_cast<const char *>(&readData2[offset]), GetSoftwareVersion().c_str()) == 0);
        offset += softwareVersionSize;
    }

    if (processNameSize)
    {
        BOOST_TEST(strcmp(reinterpret_cast<const char *>(&readData2[offset]), GetProcessName().c_str()) == 0);
        offset += processNameSize;
    }

    if (packetEntries)
    {
        BOOST_TEST((ReadUint32(readBuffer2, offset) >> 16) == packetEntries);
        offset += sizeUint32;
        for (uint32_t i = 0; i < packetEntries - 1; ++i)
        {
            BOOST_TEST(((ReadUint32(readBuffer2, offset) >> 26) & 0x3F) == 0);
            BOOST_TEST(((ReadUint32(readBuffer2, offset) >> 16) & 0x3FF) == i);
            offset += sizeUint32;
            BOOST_TEST(ReadUint32(readBuffer2, offset) == EncodeVersion(1, 0, 0));
            offset += sizeUint32;
        }

        BOOST_TEST(((ReadUint32(readBuffer2, offset) >> 26) & 0x3F) == 1);
        BOOST_TEST(((ReadUint32(readBuffer2, offset) >> 16) & 0x3FF) == 0);
        offset += sizeUint32;
        BOOST_TEST(ReadUint32(readBuffer2, offset) == EncodeVersion(1, 0, 0));
        offset += sizeUint32;
    }

    BOOST_TEST(offset == totalLength);
}

BOOST_AUTO_TEST_CASE(CreateDeviceRecordTest)
{
    ProfilingStateMachine profilingStateMachine;

    MockBufferManager mockBuffer(0);
    SendCounterPacketTest sendCounterPacketTest(profilingStateMachine, mockBuffer);

    // Create a device for testing
    uint16_t deviceUid = 27;
    const std::string deviceName = "some_device";
    uint16_t deviceCores = 3;
    const DevicePtr device = std::make_unique<Device>(deviceUid, deviceName, deviceCores);

    // Create a device record
    SendCounterPacket::DeviceRecord deviceRecord;
    std::string errorMessage;
    bool result = sendCounterPacketTest.CreateDeviceRecordTest(device, deviceRecord, errorMessage);

    BOOST_CHECK(result);
    BOOST_CHECK(errorMessage.empty());
    BOOST_CHECK(deviceRecord.size() == 6); // Size in words: header [2] + device name [4]

    uint16_t deviceRecordWord0[]
    {
        static_cast<uint16_t>(deviceRecord[0] >> 16),
        static_cast<uint16_t>(deviceRecord[0])
    };
    BOOST_CHECK(deviceRecordWord0[0] == deviceUid); // uid
    BOOST_CHECK(deviceRecordWord0[1] == deviceCores); // cores
    BOOST_CHECK(deviceRecord[1] == 0); // name_offset
    BOOST_CHECK(deviceRecord[2] == deviceName.size() + 1); // The length of the SWTrace string (name)
    BOOST_CHECK(std::memcmp(deviceRecord.data() + 3, deviceName.data(), deviceName.size()) == 0); // name
}

BOOST_AUTO_TEST_CASE(CreateInvalidDeviceRecordTest)
{
    ProfilingStateMachine profilingStateMachine;

    MockBufferManager mockBuffer(0);
    SendCounterPacketTest sendCounterPacketTest(profilingStateMachine, mockBuffer);

    // Create a device for testing
    uint16_t deviceUid = 27;
    const std::string deviceName = "some€£invalid‡device";
    uint16_t deviceCores = 3;
    const DevicePtr device = std::make_unique<Device>(deviceUid, deviceName, deviceCores);

    // Create a device record
    SendCounterPacket::DeviceRecord deviceRecord;
    std::string errorMessage;
    bool result = sendCounterPacketTest.CreateDeviceRecordTest(device, deviceRecord, errorMessage);

    BOOST_CHECK(!result);
    BOOST_CHECK(!errorMessage.empty());
    BOOST_CHECK(deviceRecord.empty());
}

BOOST_AUTO_TEST_CASE(CreateCounterSetRecordTest)
{
    ProfilingStateMachine profilingStateMachine;

    MockBufferManager mockBuffer(0);
    SendCounterPacketTest sendCounterPacketTest(profilingStateMachine, mockBuffer);

    // Create a counter set for testing
    uint16_t counterSetUid = 27;
    const std::string counterSetName = "some_counter_set";
    uint16_t counterSetCount = 3421;
    const CounterSetPtr counterSet = std::make_unique<CounterSet>(counterSetUid, counterSetName, counterSetCount);

    // Create a counter set record
    SendCounterPacket::CounterSetRecord counterSetRecord;
    std::string errorMessage;
    bool result = sendCounterPacketTest.CreateCounterSetRecordTest(counterSet, counterSetRecord, errorMessage);

    BOOST_CHECK(result);
    BOOST_CHECK(errorMessage.empty());
    BOOST_CHECK(counterSetRecord.size() == 8); // Size in words: header [2] + counter set name [6]

    uint16_t counterSetRecordWord0[]
    {
        static_cast<uint16_t>(counterSetRecord[0] >> 16),
        static_cast<uint16_t>(counterSetRecord[0])
    };
    BOOST_CHECK(counterSetRecordWord0[0] == counterSetUid); // uid
    BOOST_CHECK(counterSetRecordWord0[1] == counterSetCount); // cores
    BOOST_CHECK(counterSetRecord[1] == 0); // name_offset
    BOOST_CHECK(counterSetRecord[2] == counterSetName.size() + 1); // The length of the SWTrace string (name)
    BOOST_CHECK(std::memcmp(counterSetRecord.data() + 3, counterSetName.data(), counterSetName.size()) == 0); // name
}

BOOST_AUTO_TEST_CASE(CreateInvalidCounterSetRecordTest)
{
    ProfilingStateMachine profilingStateMachine;

    MockBufferManager mockBuffer(0);
    SendCounterPacketTest sendCounterPacketTest(profilingStateMachine, mockBuffer);

    // Create a counter set for testing
    uint16_t counterSetUid = 27;
    const std::string counterSetName = "some invalid_counter€£set";
    uint16_t counterSetCount = 3421;
    const CounterSetPtr counterSet = std::make_unique<CounterSet>(counterSetUid, counterSetName, counterSetCount);

    // Create a counter set record
    SendCounterPacket::CounterSetRecord counterSetRecord;
    std::string errorMessage;
    bool result = sendCounterPacketTest.CreateCounterSetRecordTest(counterSet, counterSetRecord, errorMessage);

    BOOST_CHECK(!result);
    BOOST_CHECK(!errorMessage.empty());
    BOOST_CHECK(counterSetRecord.empty());
}

BOOST_AUTO_TEST_CASE(CreateEventRecordTest)
{
    ProfilingStateMachine profilingStateMachine;

    MockBufferManager mockBuffer(0);
    SendCounterPacketTest sendCounterPacketTest(profilingStateMachine, mockBuffer);

    // Create a counter for testing
    uint16_t counterUid = 7256;
    uint16_t maxCounterUid = 132;
    uint16_t deviceUid = 132;
    uint16_t counterSetUid = 4497;
    uint16_t counterClass = 1;
    uint16_t counterInterpolation = 1;
    double counterMultiplier = 1234.567f;
    const std::string counterName = "some_valid_counter";
    const std::string counterDescription = "a_counter_for_testing";
    const std::string counterUnits = "Mrads2";
    const CounterPtr counter = std::make_unique<Counter>(counterUid,
                                                         maxCounterUid,
                                                         counterClass,
                                                         counterInterpolation,
                                                         counterMultiplier,
                                                         counterName,
                                                         counterDescription,
                                                         counterUnits,
                                                         deviceUid,
                                                         counterSetUid);
    BOOST_ASSERT(counter);

    // Create an event record
    SendCounterPacket::EventRecord eventRecord;
    std::string errorMessage;
    bool result = sendCounterPacketTest.CreateEventRecordTest(counter, eventRecord, errorMessage);

    BOOST_CHECK(result);
    BOOST_CHECK(errorMessage.empty());
    BOOST_CHECK(eventRecord.size() == 24); // Size in words: header [8] + counter name [6] + description [7] + units [3]

    uint16_t eventRecordWord0[]
    {
        static_cast<uint16_t>(eventRecord[0] >> 16),
        static_cast<uint16_t>(eventRecord[0])
    };
    uint16_t eventRecordWord1[]
    {
        static_cast<uint16_t>(eventRecord[1] >> 16),
        static_cast<uint16_t>(eventRecord[1])
    };
    uint16_t eventRecordWord2[]
    {
        static_cast<uint16_t>(eventRecord[2] >> 16),
        static_cast<uint16_t>(eventRecord[2])
    };
    uint32_t eventRecordWord34[]
    {
        eventRecord[3],
        eventRecord[4]
    };
    BOOST_CHECK(eventRecordWord0[0] == maxCounterUid); // max_counter_uid
    BOOST_CHECK(eventRecordWord0[1] == counterUid); // counter_uid
    BOOST_CHECK(eventRecordWord1[0] == deviceUid); // device
    BOOST_CHECK(eventRecordWord1[1] == counterSetUid); // counter_set
    BOOST_CHECK(eventRecordWord2[0] == counterClass); // class
    BOOST_CHECK(eventRecordWord2[1] == counterInterpolation); // interpolation
    BOOST_CHECK(std::memcmp(eventRecordWord34, &counterMultiplier, sizeof(counterMultiplier)) == 0); // multiplier

    ARMNN_NO_CONVERSION_WARN_BEGIN
    uint32_t counterNameOffset = 0; // The name is the first item in pool
    uint32_t counterDescriptionOffset = counterNameOffset + // Counter name offset
                                        4u + // Counter name length (uint32_t)
                                        counterName.size() + // 18u
                                        1u + // Null-terminator
                                        1u; // Rounding to the next word
    size_t counterUnitsOffset = counterDescriptionOffset + // Counter description offset
                                4u + // Counter description length (uint32_t)
                                counterDescription.size() + // 21u
                                1u + // Null-terminator
                                2u; // Rounding to the next word
    ARMNN_NO_CONVERSION_WARN_END

    BOOST_CHECK(eventRecord[5] == counterNameOffset); // name_offset
    BOOST_CHECK(eventRecord[6] == counterDescriptionOffset); // description_offset
    BOOST_CHECK(eventRecord[7] == counterUnitsOffset); // units_offset

    auto eventRecordPool = reinterpret_cast<unsigned char*>(eventRecord.data() + 8u); // The start of the pool
    size_t uint32_t_size = sizeof(uint32_t);

    // The length of the SWTrace string (name)
    BOOST_CHECK(eventRecordPool[counterNameOffset] == counterName.size() + 1);
    // The counter name
    BOOST_CHECK(std::memcmp(eventRecordPool +
                            counterNameOffset + // Offset
                            uint32_t_size /* The length of the name */,
                            counterName.data(),
                            counterName.size()) == 0); // name
    // The null-terminator at the end of the name
    BOOST_CHECK(eventRecordPool[counterNameOffset + uint32_t_size + counterName.size()] == '\0');

    // The length of the SWTrace string (description)
    BOOST_CHECK(eventRecordPool[counterDescriptionOffset] == counterDescription.size() + 1);
    // The counter description
    BOOST_CHECK(std::memcmp(eventRecordPool +
                            counterDescriptionOffset + // Offset
                            uint32_t_size /* The length of the description */,
                            counterDescription.data(),
                            counterDescription.size()) == 0); // description
    // The null-terminator at the end of the description
    BOOST_CHECK(eventRecordPool[counterDescriptionOffset + uint32_t_size + counterDescription.size()] == '\0');

    // The length of the SWTrace namestring (units)
    BOOST_CHECK(eventRecordPool[counterUnitsOffset] == counterUnits.size() + 1);
    // The counter units
    BOOST_CHECK(std::memcmp(eventRecordPool +
                            counterUnitsOffset + // Offset
                            uint32_t_size /* The length of the units */,
                            counterUnits.data(),
                            counterUnits.size()) == 0); // units
    // The null-terminator at the end of the units
    BOOST_CHECK(eventRecordPool[counterUnitsOffset + uint32_t_size + counterUnits.size()] == '\0');
}

BOOST_AUTO_TEST_CASE(CreateEventRecordNoUnitsTest)
{
    ProfilingStateMachine profilingStateMachine;

    MockBufferManager mockBuffer(0);
    SendCounterPacketTest sendCounterPacketTest(profilingStateMachine, mockBuffer);

    // Create a counter for testing
    uint16_t counterUid = 44312;
    uint16_t maxCounterUid = 345;
    uint16_t deviceUid = 101;
    uint16_t counterSetUid = 34035;
    uint16_t counterClass = 0;
    uint16_t counterInterpolation = 1;
    double counterMultiplier = 4435.0023f;
    const std::string counterName = "some_valid_counter";
    const std::string counterDescription = "a_counter_for_testing";
    const CounterPtr counter = std::make_unique<Counter>(counterUid,
                                                         maxCounterUid,
                                                         counterClass,
                                                         counterInterpolation,
                                                         counterMultiplier,
                                                         counterName,
                                                         counterDescription,
                                                         "",
                                                         deviceUid,
                                                         counterSetUid);
    BOOST_ASSERT(counter);

    // Create an event record
    SendCounterPacket::EventRecord eventRecord;
    std::string errorMessage;
    bool result = sendCounterPacketTest.CreateEventRecordTest(counter, eventRecord, errorMessage);

    BOOST_CHECK(result);
    BOOST_CHECK(errorMessage.empty());
    BOOST_CHECK(eventRecord.size() == 21); // Size in words: header [8] + counter name [6] + description [7]

    uint16_t eventRecordWord0[]
    {
        static_cast<uint16_t>(eventRecord[0] >> 16),
        static_cast<uint16_t>(eventRecord[0])
    };
    uint16_t eventRecordWord1[]
    {
        static_cast<uint16_t>(eventRecord[1] >> 16),
        static_cast<uint16_t>(eventRecord[1])
    };
    uint16_t eventRecordWord2[]
    {
        static_cast<uint16_t>(eventRecord[2] >> 16),
        static_cast<uint16_t>(eventRecord[2])
    };
    uint32_t eventRecordWord34[]
    {
        eventRecord[3],
        eventRecord[4]
    };
    BOOST_CHECK(eventRecordWord0[0] == maxCounterUid); // max_counter_uid
    BOOST_CHECK(eventRecordWord0[1] == counterUid); // counter_uid
    BOOST_CHECK(eventRecordWord1[0] == deviceUid); // device
    BOOST_CHECK(eventRecordWord1[1] == counterSetUid); // counter_set
    BOOST_CHECK(eventRecordWord2[0] == counterClass); // class
    BOOST_CHECK(eventRecordWord2[1] == counterInterpolation); // interpolation
    BOOST_CHECK(std::memcmp(eventRecordWord34, &counterMultiplier, sizeof(counterMultiplier)) == 0); // multiplier

    ARMNN_NO_CONVERSION_WARN_BEGIN
    uint32_t counterNameOffset = 0; // The name is the first item in pool
    uint32_t counterDescriptionOffset = counterNameOffset + // Counter name offset
                                        4u + // Counter name length (uint32_t)
                                        counterName.size() + // 18u
                                        1u + // Null-terminator
                                        1u; // Rounding to the next word
    ARMNN_NO_CONVERSION_WARN_END

    BOOST_CHECK(eventRecord[5] == counterNameOffset); // name_offset
    BOOST_CHECK(eventRecord[6] == counterDescriptionOffset); // description_offset
    BOOST_CHECK(eventRecord[7] == 0); // units_offset

    auto eventRecordPool = reinterpret_cast<unsigned char*>(eventRecord.data() + 8u); // The start of the pool
    size_t uint32_t_size = sizeof(uint32_t);

    // The length of the SWTrace string (name)
    BOOST_CHECK(eventRecordPool[counterNameOffset] == counterName.size() + 1);
    // The counter name
    BOOST_CHECK(std::memcmp(eventRecordPool +
                            counterNameOffset + // Offset
                            uint32_t_size, // The length of the name
                            counterName.data(),
                            counterName.size()) == 0); // name
    // The null-terminator at the end of the name
    BOOST_CHECK(eventRecordPool[counterNameOffset + uint32_t_size + counterName.size()] == '\0');

    // The length of the SWTrace string (description)
    BOOST_CHECK(eventRecordPool[counterDescriptionOffset] == counterDescription.size() + 1);
    // The counter description
    BOOST_CHECK(std::memcmp(eventRecordPool +
                            counterDescriptionOffset + // Offset
                            uint32_t_size, // The length of the description
                            counterDescription.data(),
                            counterDescription.size()) == 0); // description
    // The null-terminator at the end of the description
    BOOST_CHECK(eventRecordPool[counterDescriptionOffset + uint32_t_size + counterDescription.size()] == '\0');
}

BOOST_AUTO_TEST_CASE(CreateInvalidEventRecordTest1)
{
    ProfilingStateMachine profilingStateMachine;

    MockBufferManager mockBuffer(0);
    SendCounterPacketTest sendCounterPacketTest(profilingStateMachine, mockBuffer);

    // Create a counter for testing
    uint16_t counterUid = 7256;
    uint16_t maxCounterUid = 132;
    uint16_t deviceUid = 132;
    uint16_t counterSetUid = 4497;
    uint16_t counterClass = 1;
    uint16_t counterInterpolation = 1;
    double counterMultiplier = 1234.567f;
    const std::string counterName = "some_invalid_counter £££"; // Invalid name
    const std::string counterDescription = "a_counter_for_testing";
    const std::string counterUnits = "Mrads2";
    const CounterPtr counter = std::make_unique<Counter>(counterUid,
                                                         maxCounterUid,
                                                         counterClass,
                                                         counterInterpolation,
                                                         counterMultiplier,
                                                         counterName,
                                                         counterDescription,
                                                         counterUnits,
                                                         deviceUid,
                                                         counterSetUid);
    BOOST_ASSERT(counter);

    // Create an event record
    SendCounterPacket::EventRecord eventRecord;
    std::string errorMessage;
    bool result = sendCounterPacketTest.CreateEventRecordTest(counter, eventRecord, errorMessage);

    BOOST_CHECK(!result);
    BOOST_CHECK(!errorMessage.empty());
    BOOST_CHECK(eventRecord.empty());
}

BOOST_AUTO_TEST_CASE(CreateInvalidEventRecordTest2)
{
    ProfilingStateMachine profilingStateMachine;

    MockBufferManager mockBuffer(0);
    SendCounterPacketTest sendCounterPacketTest(profilingStateMachine, mockBuffer);

    // Create a counter for testing
    uint16_t counterUid = 7256;
    uint16_t maxCounterUid = 132;
    uint16_t deviceUid = 132;
    uint16_t counterSetUid = 4497;
    uint16_t counterClass = 1;
    uint16_t counterInterpolation = 1;
    double counterMultiplier = 1234.567f;
    const std::string counterName = "some_invalid_counter";
    const std::string counterDescription = "an invalid d€scription"; // Invalid description
    const std::string counterUnits = "Mrads2";
    const CounterPtr counter = std::make_unique<Counter>(counterUid,
                                                         maxCounterUid,
                                                         counterClass,
                                                         counterInterpolation,
                                                         counterMultiplier,
                                                         counterName,
                                                         counterDescription,
                                                         counterUnits,
                                                         deviceUid,
                                                         counterSetUid);
    BOOST_ASSERT(counter);

    // Create an event record
    SendCounterPacket::EventRecord eventRecord;
    std::string errorMessage;
    bool result = sendCounterPacketTest.CreateEventRecordTest(counter, eventRecord, errorMessage);

    BOOST_CHECK(!result);
    BOOST_CHECK(!errorMessage.empty());
    BOOST_CHECK(eventRecord.empty());
}

BOOST_AUTO_TEST_CASE(CreateInvalidEventRecordTest3)
{
    ProfilingStateMachine profilingStateMachine;

    MockBufferManager mockBuffer(0);
    SendCounterPacketTest sendCounterPacketTest(profilingStateMachine, mockBuffer);

    // Create a counter for testing
    uint16_t counterUid = 7256;
    uint16_t maxCounterUid = 132;
    uint16_t deviceUid = 132;
    uint16_t counterSetUid = 4497;
    uint16_t counterClass = 1;
    uint16_t counterInterpolation = 1;
    double counterMultiplier = 1234.567f;
    const std::string counterName = "some_invalid_counter";
    const std::string counterDescription = "a valid description";
    const std::string counterUnits = "Mrad s2"; // Invalid units
    const CounterPtr counter = std::make_unique<Counter>(counterUid,
                                                         maxCounterUid,
                                                         counterClass,
                                                         counterInterpolation,
                                                         counterMultiplier,
                                                         counterName,
                                                         counterDescription,
                                                         counterUnits,
                                                         deviceUid,
                                                         counterSetUid);
    BOOST_ASSERT(counter);

    // Create an event record
    SendCounterPacket::EventRecord eventRecord;
    std::string errorMessage;
    bool result = sendCounterPacketTest.CreateEventRecordTest(counter, eventRecord, errorMessage);

    BOOST_CHECK(!result);
    BOOST_CHECK(!errorMessage.empty());
    BOOST_CHECK(eventRecord.empty());
}

BOOST_AUTO_TEST_CASE(CreateCategoryRecordTest)
{
    ProfilingStateMachine profilingStateMachine;

    MockBufferManager mockBuffer(0);
    SendCounterPacketTest sendCounterPacketTest(profilingStateMachine, mockBuffer);

    // Create a category for testing
    const std::string categoryName = "some_category";
    uint16_t deviceUid = 1302;
    uint16_t counterSetUid = 20734;
    const CategoryPtr category = std::make_unique<Category>(categoryName, deviceUid, counterSetUid);
    BOOST_ASSERT(category);
    category->m_Counters = { 11u, 23u, 5670u };

    // Create a collection of counters
    Counters counters;
    counters.insert(std::make_pair<uint16_t, CounterPtr>(11,
                                                         CounterPtr(new Counter(11,
                                                                                1234,
                                                                                0,
                                                                                1,
                                                                                534.0003f,
                                                                                "counter1",
                                                                                "the first counter",
                                                                                "millipi2",
                                                                                0,
                                                                                0))));
    counters.insert(std::make_pair<uint16_t, CounterPtr>(23,
                                                         CounterPtr(new Counter(23,
                                                                                344,
                                                                                1,
                                                                                1,
                                                                                534.0003f,
                                                                                "this is counter 2",
                                                                                "the second counter",
                                                                                "",
                                                                                0,
                                                                                0))));
    counters.insert(std::make_pair<uint16_t, CounterPtr>(5670,
                                                         CounterPtr(new Counter(5670,
                                                                                31,
                                                                                0,
                                                                                0,
                                                                                534.0003f,
                                                                                "and this is number 3",
                                                                                "the third counter",
                                                                                "blah_per_second",
                                                                                0,
                                                                                0))));
    Counter* counter1 = counters.find(11)->second.get();
    Counter* counter2 = counters.find(23)->second.get();
    Counter* counter3 = counters.find(5670)->second.get();
    BOOST_ASSERT(counter1);
    BOOST_ASSERT(counter2);
    BOOST_ASSERT(counter3);
    uint16_t categoryEventCount = boost::numeric_cast<uint16_t>(counters.size());

    // Create a category record
    SendCounterPacket::CategoryRecord categoryRecord;
    std::string errorMessage;
    bool result = sendCounterPacketTest.CreateCategoryRecordTest(category, counters, categoryRecord, errorMessage);

    BOOST_CHECK(result);
    BOOST_CHECK(errorMessage.empty());
    BOOST_CHECK(categoryRecord.size() == 80); // Size in words: header [4] + event pointer table [3] +
                                              //                category name [5] + event records [68 = 22 + 20 + 26]

    uint16_t categoryRecordWord0[]
    {
        static_cast<uint16_t>(categoryRecord[0] >> 16),
        static_cast<uint16_t>(categoryRecord[0])
    };
    uint16_t categoryRecordWord1[]
    {
        static_cast<uint16_t>(categoryRecord[1] >> 16),
        static_cast<uint16_t>(categoryRecord[1])
    };
    BOOST_CHECK(categoryRecordWord0[0] == deviceUid); // device
    BOOST_CHECK(categoryRecordWord0[1] == counterSetUid); // counter_set
    BOOST_CHECK(categoryRecordWord1[0] == categoryEventCount); // event_count
    BOOST_CHECK(categoryRecordWord1[1] == 0); // reserved

    size_t uint32_t_size = sizeof(uint32_t);

    ARMNN_NO_CONVERSION_WARN_BEGIN
    uint32_t eventPointerTableOffset = 0; // The event pointer table is the first item in pool
    uint32_t categoryNameOffset = eventPointerTableOffset + // Event pointer table offset
                                  categoryEventCount * uint32_t_size; // The size of the event pointer table
    ARMNN_NO_CONVERSION_WARN_END

    BOOST_CHECK(categoryRecord[2] == eventPointerTableOffset); // event_pointer_table_offset
    BOOST_CHECK(categoryRecord[3] == categoryNameOffset); // name_offset

    auto categoryRecordPool = reinterpret_cast<unsigned char*>(categoryRecord.data() + 4u); // The start of the pool

    // The event pointer table
    uint32_t eventRecord0Offset = categoryRecordPool[eventPointerTableOffset + 0 * uint32_t_size];
    uint32_t eventRecord1Offset = categoryRecordPool[eventPointerTableOffset + 1 * uint32_t_size];
    uint32_t eventRecord2Offset = categoryRecordPool[eventPointerTableOffset + 2 * uint32_t_size];
    BOOST_CHECK(eventRecord0Offset == 32);
    BOOST_CHECK(eventRecord1Offset == 120);
    BOOST_CHECK(eventRecord2Offset == 200);

    // The length of the SWTrace namestring (name)
    BOOST_CHECK(categoryRecordPool[categoryNameOffset] == categoryName.size() + 1);
    // The category name
    BOOST_CHECK(std::memcmp(categoryRecordPool +
                            categoryNameOffset + // Offset
                            uint32_t_size, // The length of the name
                            categoryName.data(),
                            categoryName.size()) == 0); // name
    // The null-terminator at the end of the name
    BOOST_CHECK(categoryRecordPool[categoryNameOffset + uint32_t_size + categoryName.size()] == '\0');

    // For brevity, checking only the UIDs, max counter UIDs and names of the counters in the event records,
    // as the event records already have a number of unit tests dedicated to them

    // Counter1 UID and max counter UID
    uint16_t eventRecord0Word0[2] = { 0u, 0u };
    std::memcpy(eventRecord0Word0, categoryRecordPool + eventRecord0Offset, sizeof(eventRecord0Word0));
    BOOST_CHECK(eventRecord0Word0[0] == counter1->m_Uid);
    BOOST_CHECK(eventRecord0Word0[1] == counter1->m_MaxCounterUid);

    // Counter1 name
    uint32_t counter1NameOffset = 0;
    std::memcpy(&counter1NameOffset, categoryRecordPool + eventRecord0Offset + 5u * uint32_t_size, uint32_t_size);
    BOOST_CHECK(counter1NameOffset == 0);
    // The length of the SWTrace string (name)
    BOOST_CHECK(categoryRecordPool[eventRecord0Offset + // Offset to the event record
                                   8u * uint32_t_size + // Offset to the event record pool
                                   counter1NameOffset   // Offset to the name of the counter
                                  ] == counter1->m_Name.size() + 1); // The length of the name including the
                                                                     // null-terminator
    // The counter1 name
    BOOST_CHECK(std::memcmp(categoryRecordPool + // The beginning of the category pool
                            eventRecord0Offset + // Offset to the event record
                            8u * uint32_t_size + // Offset to the event record pool
                            counter1NameOffset + // Offset to the name of the counter
                            uint32_t_size,       // The length of the name
                            counter1->m_Name.data(),
                            counter1->m_Name.size()) == 0); // name
    // The null-terminator at the end of the counter1 name
    BOOST_CHECK(categoryRecordPool[eventRecord0Offset +    // Offset to the event record
                                   8u * uint32_t_size +    // Offset to the event record pool
                                   counter1NameOffset +    // Offset to the name of the counter
                                   uint32_t_size +         // The length of the name
                                   counter1->m_Name.size() // The name of the counter
                                   ] == '\0');

    // Counter2 name
    uint32_t counter2NameOffset = 0;
    std::memcpy(&counter2NameOffset, categoryRecordPool + eventRecord1Offset + 5u * uint32_t_size, uint32_t_size);
    BOOST_CHECK(counter2NameOffset == 0);
    // The length of the SWTrace string (name)
    BOOST_CHECK(categoryRecordPool[eventRecord1Offset + // Offset to the event record
                                   8u * uint32_t_size + // Offset to the event record pool
                                   counter2NameOffset   // Offset to the name of the counter
                                  ] == counter2->m_Name.size() + 1); // The length of the name including the
                                                                     // null-terminator
    // The counter2 name
    BOOST_CHECK(std::memcmp(categoryRecordPool + // The beginning of the category pool
                            eventRecord1Offset + // Offset to the event record
                            8u * uint32_t_size + // Offset to the event record pool
                            counter2NameOffset + // Offset to the name of the counter
                            uint32_t_size,       // The length of the name
                            counter2->m_Name.data(),
                            counter2->m_Name.size()) == 0); // name
    // The null-terminator at the end of the counter2 name
    BOOST_CHECK(categoryRecordPool[eventRecord1Offset +    // Offset to the event record
                                   8u * uint32_t_size +    // Offset to the event record pool
                                   counter2NameOffset +    // Offset to the name of the counter
                                   uint32_t_size +         // The length of the name
                                   counter2->m_Name.size() // The name of the counter
                                   ] == '\0');

    // Counter3 name
    uint32_t counter3NameOffset = 0;
    std::memcpy(&counter3NameOffset, categoryRecordPool + eventRecord2Offset + 5u * uint32_t_size, uint32_t_size);
    BOOST_CHECK(counter3NameOffset == 0);
    // The length of the SWTrace string (name)
    BOOST_CHECK(categoryRecordPool[eventRecord2Offset + // Offset to the event record
                                   8u * uint32_t_size + // Offset to the event record pool
                                   counter3NameOffset   // Offset to the name of the counter
                                  ] == counter3->m_Name.size() + 1); // The length of the name including the
                                                                     // null-terminator
    // The counter3 name
    BOOST_CHECK(std::memcmp(categoryRecordPool + // The beginning of the category pool
                            eventRecord2Offset + // Offset to the event record
                            8u * uint32_t_size + // Offset to the event record pool
                            counter3NameOffset + // Offset to the name of the counter
                            uint32_t_size,       // The length of the name
                            counter3->m_Name.data(),
                            counter3->m_Name.size()) == 0); // name
    // The null-terminator at the end of the counter3 name
    BOOST_CHECK(categoryRecordPool[eventRecord2Offset +    // Offset to the event record
                                   8u * uint32_t_size +    // Offset to the event record pool
                                   counter3NameOffset +    // Offset to the name of the counter
                                   uint32_t_size +         // The length of the name
                                   counter3->m_Name.size() // The name of the counter
                                   ] == '\0');
}

BOOST_AUTO_TEST_CASE(CreateInvalidCategoryRecordTest1)
{
    ProfilingStateMachine profilingStateMachine;

    MockBufferManager mockBuffer(0);
    SendCounterPacketTest sendCounterPacketTest(profilingStateMachine, mockBuffer);

    // Create a category for testing
    const std::string categoryName = "some invalid category";
    uint16_t deviceUid = 1302;
    uint16_t counterSetUid = 20734;
    const CategoryPtr category = std::make_unique<Category>(categoryName, deviceUid, counterSetUid);
    BOOST_CHECK(category);

    // Create a category record
    Counters counters;
    SendCounterPacket::CategoryRecord categoryRecord;
    std::string errorMessage;
    bool result = sendCounterPacketTest.CreateCategoryRecordTest(category, counters, categoryRecord, errorMessage);

    BOOST_CHECK(!result);
    BOOST_CHECK(!errorMessage.empty());
    BOOST_CHECK(categoryRecord.empty());
}

BOOST_AUTO_TEST_CASE(CreateInvalidCategoryRecordTest2)
{
    ProfilingStateMachine profilingStateMachine;

    MockBufferManager mockBuffer(0);
    SendCounterPacketTest sendCounterPacketTest(profilingStateMachine, mockBuffer);

    // Create a category for testing
    const std::string categoryName = "some_category";
    uint16_t deviceUid = 1302;
    uint16_t counterSetUid = 20734;
    const CategoryPtr category = std::make_unique<Category>(categoryName, deviceUid, counterSetUid);
    BOOST_CHECK(category);
    category->m_Counters = { 11u, 23u, 5670u };

    // Create a collection of counters
    Counters counters;
    counters.insert(std::make_pair<uint16_t, CounterPtr>(11,
                                                         CounterPtr(new Counter(11,
                                                                                1234,
                                                                                0,
                                                                                1,
                                                                                534.0003f,
                                                                                "count€r1", // Invalid name
                                                                                "the first counter",
                                                                                "millipi2",
                                                                                0,
                                                                                0))));

    Counter* counter1 = counters.find(11)->second.get();
    BOOST_CHECK(counter1);

    // Create a category record
    SendCounterPacket::CategoryRecord categoryRecord;
    std::string errorMessage;
    bool result = sendCounterPacketTest.CreateCategoryRecordTest(category, counters, categoryRecord, errorMessage);

    BOOST_CHECK(!result);
    BOOST_CHECK(!errorMessage.empty());
    BOOST_CHECK(categoryRecord.empty());
}

BOOST_AUTO_TEST_CASE(SendCounterDirectoryPacketTest1)
{
    ProfilingStateMachine profilingStateMachine;

    // The counter directory used for testing
    CounterDirectory counterDirectory;

    // Register a device
    const std::string device1Name = "device1";
    const Device* device1 = nullptr;
    BOOST_CHECK_NO_THROW(device1 = counterDirectory.RegisterDevice(device1Name, 3));
    BOOST_CHECK(counterDirectory.GetDeviceCount() == 1);
    BOOST_CHECK(device1);

    // Register a device
    const std::string device2Name = "device2";
    const Device* device2 = nullptr;
    BOOST_CHECK_NO_THROW(device2 = counterDirectory.RegisterDevice(device2Name));
    BOOST_CHECK(counterDirectory.GetDeviceCount() == 2);
    BOOST_CHECK(device2);

    // Buffer with not enough space
    MockBufferManager mockBuffer(10);
    SendCounterPacket sendCounterPacket(profilingStateMachine, mockBuffer);
    BOOST_CHECK_THROW(sendCounterPacket.SendCounterDirectoryPacket(counterDirectory),
                      armnn::profiling::BufferExhaustion);
}

BOOST_AUTO_TEST_CASE(SendCounterDirectoryPacketTest2)
{
    ProfilingStateMachine profilingStateMachine;

    // The counter directory used for testing
    CounterDirectory counterDirectory;

    // Register a device
    const std::string device1Name = "device1";
    const Device* device1 = nullptr;
    BOOST_CHECK_NO_THROW(device1 = counterDirectory.RegisterDevice(device1Name, 3));
    BOOST_CHECK(counterDirectory.GetDeviceCount() == 1);
    BOOST_CHECK(device1);

    // Register a device
    const std::string device2Name = "device2";
    const Device* device2 = nullptr;
    BOOST_CHECK_NO_THROW(device2 = counterDirectory.RegisterDevice(device2Name));
    BOOST_CHECK(counterDirectory.GetDeviceCount() == 2);
    BOOST_CHECK(device2);

    // Register a counter set
    const std::string counterSet1Name = "counterset1";
    const CounterSet* counterSet1 = nullptr;
    BOOST_CHECK_NO_THROW(counterSet1 = counterDirectory.RegisterCounterSet(counterSet1Name));
    BOOST_CHECK(counterDirectory.GetCounterSetCount() == 1);
    BOOST_CHECK(counterSet1);

    // Register a category associated to "device1" and "counterset1"
    const std::string category1Name = "category1";
    const Category* category1 = nullptr;
    BOOST_CHECK_NO_THROW(category1 = counterDirectory.RegisterCategory(category1Name,
                                                                       device1->m_Uid,
                                                                       counterSet1->m_Uid));
    BOOST_CHECK(counterDirectory.GetCategoryCount() == 1);
    BOOST_CHECK(category1);

    // Register a category not associated to "device2" but no counter set
    const std::string category2Name = "category2";
    const Category* category2 = nullptr;
    BOOST_CHECK_NO_THROW(category2 = counterDirectory.RegisterCategory(category2Name,
                                                                       device2->m_Uid));
    BOOST_CHECK(counterDirectory.GetCategoryCount() == 2);
    BOOST_CHECK(category2);

    // Register a counter associated to "category1"
    const Counter* counter1 = nullptr;
    BOOST_CHECK_NO_THROW(counter1 = counterDirectory.RegisterCounter(category1Name,
                                                                     0,
                                                                     1,
                                                                     123.45f,
                                                                     "counter1",
                                                                     "counter1description",
                                                                     std::string("counter1units")));
    BOOST_CHECK(counterDirectory.GetCounterCount() == 3);
    BOOST_CHECK(counter1);

    // Register a counter associated to "category1"
    const Counter* counter2 = nullptr;
    BOOST_CHECK_NO_THROW(counter2 = counterDirectory.RegisterCounter(category1Name,
                                                                     1,
                                                                     0,
                                                                     330.1245656765f,
                                                                     "counter2",
                                                                     "counter2description",
                                                                     std::string("counter2units"),
                                                                     armnn::EmptyOptional(),
                                                                     device2->m_Uid,
                                                                     0));
    BOOST_CHECK(counterDirectory.GetCounterCount() == 4);
    BOOST_CHECK(counter2);

    // Register a counter associated to "category2"
    const Counter* counter3 = nullptr;
    BOOST_CHECK_NO_THROW(counter3 = counterDirectory.RegisterCounter(category2Name,
                                                                     1,
                                                                     1,
                                                                     0.0000045399f,
                                                                     "counter3",
                                                                     "counter3description",
                                                                     armnn::EmptyOptional(),
                                                                     5,
                                                                     device2->m_Uid,
                                                                     counterSet1->m_Uid));
    BOOST_CHECK(counterDirectory.GetCounterCount() == 9);
    BOOST_CHECK(counter3);

    // Buffer with enough space
    MockBufferManager mockBuffer(1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, mockBuffer);
    BOOST_CHECK_NO_THROW(sendCounterPacket.SendCounterDirectoryPacket(counterDirectory));

    // Get the readable buffer
    auto readBuffer = mockBuffer.GetReadableBuffer();

    // Check the packet header
    uint32_t packetHeaderWord0 = ReadUint32(readBuffer, 0);
    uint32_t packetHeaderWord1 = ReadUint32(readBuffer, 4);
    BOOST_TEST(((packetHeaderWord0 >> 26) & 0x3F) == 0);  // packet_family
    BOOST_TEST(((packetHeaderWord0 >> 16) & 0x3FF) == 2); // packet_id
    BOOST_TEST(packetHeaderWord1 == 936);                 // data_length

    // Check the body header
    uint32_t bodyHeaderWord0 = ReadUint32(readBuffer,  8);
    uint32_t bodyHeaderWord1 = ReadUint32(readBuffer, 12);
    uint32_t bodyHeaderWord2 = ReadUint32(readBuffer, 16);
    uint32_t bodyHeaderWord3 = ReadUint32(readBuffer, 20);
    uint32_t bodyHeaderWord4 = ReadUint32(readBuffer, 24);
    uint32_t bodyHeaderWord5 = ReadUint32(readBuffer, 28);
    uint16_t deviceRecordCount     = static_cast<uint16_t>(bodyHeaderWord0 >> 16);
    uint16_t counterSetRecordCount = static_cast<uint16_t>(bodyHeaderWord2 >> 16);
    uint16_t categoryRecordCount   = static_cast<uint16_t>(bodyHeaderWord4 >> 16);
    BOOST_TEST(deviceRecordCount == 2);     // device_records_count
    BOOST_TEST(bodyHeaderWord1 == 0);       // device_records_pointer_table_offset
    BOOST_TEST(counterSetRecordCount == 1); // counter_set_count
    BOOST_TEST(bodyHeaderWord3 == 8);       // counter_set_pointer_table_offset
    BOOST_TEST(categoryRecordCount == 2);   // categories_count
    BOOST_TEST(bodyHeaderWord5 == 12);      // categories_pointer_table_offset

    // Check the device records pointer table
    uint32_t deviceRecordOffset0 = ReadUint32(readBuffer, 32);
    uint32_t deviceRecordOffset1 = ReadUint32(readBuffer, 36);
    BOOST_TEST(deviceRecordOffset0 ==  0); // Device record offset for "device1"
    BOOST_TEST(deviceRecordOffset1 == 20); // Device record offset for "device2"

    // Check the counter set pointer table
    uint32_t counterSetRecordOffset0 = ReadUint32(readBuffer, 40);
    BOOST_TEST(counterSetRecordOffset0 == 40); // Counter set record offset for "counterset1"

    // Check the category pointer table
    uint32_t categoryRecordOffset0 = ReadUint32(readBuffer, 44);
    uint32_t categoryRecordOffset1 = ReadUint32(readBuffer, 48);
    BOOST_TEST(categoryRecordOffset0 ==  64); // Category record offset for "category1"
    BOOST_TEST(categoryRecordOffset1 == 476); // Category record offset for "category2"

    // Get the device record pool offset
    uint32_t uint32_t_size = sizeof(uint32_t);
    uint32_t packetBodyPoolOffset = 2u * uint32_t_size +                    // packet_header
                                    6u * uint32_t_size +                    // body_header
                                    deviceRecordCount * uint32_t_size +     // Size of device_records_pointer_table
                                    counterSetRecordCount * uint32_t_size + // Size of counter_set_pointer_table
                                    categoryRecordCount * uint32_t_size;    // Size of categories_pointer_table

    // Device record structure/collection used for testing
    struct DeviceRecord
    {
        uint16_t    uid;
        uint16_t    cores;
        uint32_t    name_offset;
        uint32_t    name_length;
        std::string name;
    };
    std::vector<DeviceRecord> deviceRecords;
    uint32_t deviceRecordsPointerTableOffset = 2u * uint32_t_size + // packet_header
                                               6u * uint32_t_size + // body_header
                                               bodyHeaderWord1;     // device_records_pointer_table_offset

    const unsigned char* readData = readBuffer->GetReadableData();

    for (uint32_t i = 0; i < deviceRecordCount; i++)
    {
        // Get the device record offset
        uint32_t deviceRecordOffset = ReadUint32(readBuffer, deviceRecordsPointerTableOffset + i * uint32_t_size);

        // Collect the data for the device record
        uint32_t deviceRecordWord0 = ReadUint32(readBuffer,
                                                packetBodyPoolOffset + deviceRecordOffset + 0 * uint32_t_size);
        uint32_t deviceRecordWord1 = ReadUint32(readBuffer,
                                                packetBodyPoolOffset + deviceRecordOffset + 1 * uint32_t_size);
        DeviceRecord deviceRecord;
        deviceRecord.uid = static_cast<uint16_t>(deviceRecordWord0 >> 16); // uid
        deviceRecord.cores = static_cast<uint16_t>(deviceRecordWord0);     // cores
        deviceRecord.name_offset = deviceRecordWord1;                      // name_offset

        uint32_t deviceRecordPoolOffset = packetBodyPoolOffset +    // Packet body offset
                                          deviceRecordOffset +      // Device record offset
                                          2 * uint32_t_size +       // Device record header
                                          deviceRecord.name_offset; // Device name offset
        uint32_t deviceRecordNameLength = ReadUint32(readBuffer, deviceRecordPoolOffset);
        deviceRecord.name_length = deviceRecordNameLength; // name_length
        unsigned char deviceRecordNameNullTerminator = // name null-terminator
                ReadUint8(readBuffer, deviceRecordPoolOffset + uint32_t_size + deviceRecordNameLength - 1);
        BOOST_CHECK(deviceRecordNameNullTerminator == '\0');
        std::vector<unsigned char> deviceRecordNameBuffer(deviceRecord.name_length - 1);
        std::memcpy(deviceRecordNameBuffer.data(),
                    readData + deviceRecordPoolOffset + uint32_t_size, deviceRecordNameBuffer.size());
        deviceRecord.name.assign(deviceRecordNameBuffer.begin(), deviceRecordNameBuffer.end()); // name

        deviceRecords.push_back(deviceRecord);
    }

    // Check that the device records are correct
    BOOST_CHECK(deviceRecords.size() == 2);
    for (const DeviceRecord& deviceRecord : deviceRecords)
    {
        const Device* device = counterDirectory.GetDevice(deviceRecord.uid);
        BOOST_CHECK(device);
        BOOST_CHECK(device->m_Uid   == deviceRecord.uid);
        BOOST_CHECK(device->m_Cores == deviceRecord.cores);
        BOOST_CHECK(device->m_Name  == deviceRecord.name);
    }

    // Counter set record structure/collection used for testing
    struct CounterSetRecord
    {
        uint16_t    uid;
        uint16_t    count;
        uint32_t    name_offset;
        uint32_t    name_length;
        std::string name;
    };
    std::vector<CounterSetRecord> counterSetRecords;
    uint32_t counterSetRecordsPointerTableOffset = 2u * uint32_t_size + // packet_header
                                                   6u * uint32_t_size + // body_header
                                                   bodyHeaderWord3;     // counter_set_pointer_table_offset
    for (uint32_t i = 0; i < counterSetRecordCount; i++)
    {
        // Get the counter set record offset
        uint32_t counterSetRecordOffset = ReadUint32(readBuffer,
                                                     counterSetRecordsPointerTableOffset + i * uint32_t_size);

        // Collect the data for the counter set record
        uint32_t counterSetRecordWord0 = ReadUint32(readBuffer,
                                                    packetBodyPoolOffset + counterSetRecordOffset + 0 * uint32_t_size);
        uint32_t counterSetRecordWord1 = ReadUint32(readBuffer,
                                                    packetBodyPoolOffset + counterSetRecordOffset + 1 * uint32_t_size);
        CounterSetRecord counterSetRecord;
        counterSetRecord.uid = static_cast<uint16_t>(counterSetRecordWord0 >> 16); // uid
        counterSetRecord.count = static_cast<uint16_t>(counterSetRecordWord0);     // count
        counterSetRecord.name_offset = counterSetRecordWord1;                      // name_offset

        uint32_t counterSetRecordPoolOffset = packetBodyPoolOffset +        // Packet body offset
                                              counterSetRecordOffset +      // Counter set record offset
                                              2 * uint32_t_size +           // Counter set record header
                                              counterSetRecord.name_offset; // Counter set name offset
        uint32_t counterSetRecordNameLength = ReadUint32(readBuffer, counterSetRecordPoolOffset);
        counterSetRecord.name_length = counterSetRecordNameLength; // name_length
        unsigned char counterSetRecordNameNullTerminator = // name null-terminator
                ReadUint8(readBuffer, counterSetRecordPoolOffset + uint32_t_size + counterSetRecordNameLength - 1);
        BOOST_CHECK(counterSetRecordNameNullTerminator == '\0');
        std::vector<unsigned char> counterSetRecordNameBuffer(counterSetRecord.name_length - 1);
        std::memcpy(counterSetRecordNameBuffer.data(),
                    readData + counterSetRecordPoolOffset + uint32_t_size, counterSetRecordNameBuffer.size());
        counterSetRecord.name.assign(counterSetRecordNameBuffer.begin(), counterSetRecordNameBuffer.end()); // name

        counterSetRecords.push_back(counterSetRecord);
    }

    // Check that the counter set records are correct
    BOOST_CHECK(counterSetRecords.size() == 1);
    for (const CounterSetRecord& counterSetRecord : counterSetRecords)
    {
        const CounterSet* counterSet = counterDirectory.GetCounterSet(counterSetRecord.uid);
        BOOST_CHECK(counterSet);
        BOOST_CHECK(counterSet->m_Uid   == counterSetRecord.uid);
        BOOST_CHECK(counterSet->m_Count == counterSetRecord.count);
        BOOST_CHECK(counterSet->m_Name  == counterSetRecord.name);
    }

    // Event record structure/collection used for testing
    struct EventRecord
    {
        uint16_t    counter_uid;
        uint16_t    max_counter_uid;
        uint16_t    device;
        uint16_t    counter_set;
        uint16_t    counter_class;
        uint16_t    interpolation;
        double      multiplier;
        uint32_t    name_offset;
        uint32_t    name_length;
        std::string name;
        uint32_t    description_offset;
        uint32_t    description_length;
        std::string description;
        uint32_t    units_offset;
        uint32_t    units_length;
        std::string units;
    };
    // Category record structure/collection used for testing
    struct CategoryRecord
    {
        uint16_t                 device;
        uint16_t                 counter_set;
        uint16_t                 event_count;
        uint32_t                 event_pointer_table_offset;
        uint32_t                 name_offset;
        uint32_t                 name_length;
        std::string              name;
        std::vector<uint32_t>    event_pointer_table;
        std::vector<EventRecord> event_records;
    };
    std::vector<CategoryRecord> categoryRecords;
    uint32_t categoryRecordsPointerTableOffset = 2u * uint32_t_size + // packet_header
                                                 6u * uint32_t_size + // body_header
                                                 bodyHeaderWord5;     // categories_pointer_table_offset
    for (uint32_t i = 0; i < categoryRecordCount; i++)
    {
        // Get the category record offset
        uint32_t categoryRecordOffset = ReadUint32(readBuffer, categoryRecordsPointerTableOffset + i * uint32_t_size);

        // Collect the data for the category record
        uint32_t categoryRecordWord0 = ReadUint32(readBuffer,
                                                  packetBodyPoolOffset + categoryRecordOffset + 0 * uint32_t_size);
        uint32_t categoryRecordWord1 = ReadUint32(readBuffer,
                                                  packetBodyPoolOffset + categoryRecordOffset + 1 * uint32_t_size);
        uint32_t categoryRecordWord2 = ReadUint32(readBuffer,
                                                  packetBodyPoolOffset + categoryRecordOffset + 2 * uint32_t_size);
        uint32_t categoryRecordWord3 = ReadUint32(readBuffer,
                                                  packetBodyPoolOffset + categoryRecordOffset + 3 * uint32_t_size);
        CategoryRecord categoryRecord;
        categoryRecord.device = static_cast<uint16_t>(categoryRecordWord0 >> 16);      // device
        categoryRecord.counter_set = static_cast<uint16_t>(categoryRecordWord0);       // counter_set
        categoryRecord.event_count = static_cast<uint16_t>(categoryRecordWord1 >> 16); // event_count
        categoryRecord.event_pointer_table_offset = categoryRecordWord2;               // event_pointer_table_offset
        categoryRecord.name_offset = categoryRecordWord3;                              // name_offset

        uint32_t categoryRecordPoolOffset = packetBodyPoolOffset +      // Packet body offset
                                            categoryRecordOffset +      // Category record offset
                                            4 * uint32_t_size;          // Category record header

        uint32_t categoryRecordNameLength = ReadUint32(readBuffer,
                                                       categoryRecordPoolOffset + categoryRecord.name_offset);
        categoryRecord.name_length = categoryRecordNameLength; // name_length
        unsigned char categoryRecordNameNullTerminator =
                ReadUint8(readBuffer,
                          categoryRecordPoolOffset +
                          categoryRecord.name_offset +
                          uint32_t_size +
                          categoryRecordNameLength - 1); // name null-terminator
        BOOST_CHECK(categoryRecordNameNullTerminator == '\0');
        std::vector<unsigned char> categoryRecordNameBuffer(categoryRecord.name_length - 1);
        std::memcpy(categoryRecordNameBuffer.data(),
                    readData +
                    categoryRecordPoolOffset +
                    categoryRecord.name_offset +
                    uint32_t_size,
                    categoryRecordNameBuffer.size());
        categoryRecord.name.assign(categoryRecordNameBuffer.begin(), categoryRecordNameBuffer.end()); // name

        categoryRecord.event_pointer_table.resize(categoryRecord.event_count);
        for (uint32_t eventIndex = 0; eventIndex < categoryRecord.event_count; eventIndex++)
        {
            uint32_t eventRecordOffset = ReadUint32(readBuffer,
                                                    categoryRecordPoolOffset +
                                                    categoryRecord.event_pointer_table_offset +
                                                    eventIndex * uint32_t_size);
            categoryRecord.event_pointer_table[eventIndex] = eventRecordOffset;

            // Collect the data for the event record
            uint32_t eventRecordWord0  = ReadUint32(readBuffer,
                                                    categoryRecordPoolOffset + eventRecordOffset + 0 * uint32_t_size);
            uint32_t eventRecordWord1  = ReadUint32(readBuffer,
                                                    categoryRecordPoolOffset + eventRecordOffset + 1 * uint32_t_size);
            uint32_t eventRecordWord2  = ReadUint32(readBuffer,
                                                    categoryRecordPoolOffset + eventRecordOffset + 2 * uint32_t_size);
            uint64_t eventRecordWord34 = ReadUint64(readBuffer,
                                                    categoryRecordPoolOffset + eventRecordOffset + 3 * uint32_t_size);
            uint32_t eventRecordWord5 =  ReadUint32(readBuffer,
                                                    categoryRecordPoolOffset + eventRecordOffset + 5 * uint32_t_size);
            uint32_t eventRecordWord6 = ReadUint32(readBuffer,
                                                   categoryRecordPoolOffset + eventRecordOffset + 6 * uint32_t_size);
            uint32_t eventRecordWord7 = ReadUint32(readBuffer,
                                                   categoryRecordPoolOffset + eventRecordOffset + 7 * uint32_t_size);
            EventRecord eventRecord;
            eventRecord.counter_uid = static_cast<uint16_t>(eventRecordWord0);                     // counter_uid
            eventRecord.max_counter_uid = static_cast<uint16_t>(eventRecordWord0 >> 16);           // max_counter_uid
            eventRecord.device = static_cast<uint16_t>(eventRecordWord1 >> 16);                    // device
            eventRecord.counter_set = static_cast<uint16_t>(eventRecordWord1);                     // counter_set
            eventRecord.counter_class = static_cast<uint16_t>(eventRecordWord2 >> 16);             // class
            eventRecord.interpolation = static_cast<uint16_t>(eventRecordWord2);                   // interpolation
            std::memcpy(&eventRecord.multiplier, &eventRecordWord34, sizeof(eventRecord.multiplier)); // multiplier
            eventRecord.name_offset = static_cast<uint32_t>(eventRecordWord5);                     // name_offset
            eventRecord.description_offset = static_cast<uint32_t>(eventRecordWord6);              // description_offset
            eventRecord.units_offset = static_cast<uint32_t>(eventRecordWord7);                    // units_offset

            uint32_t eventRecordPoolOffset = categoryRecordPoolOffset + // Category record pool offset
                                             eventRecordOffset +        // Event record offset
                                             8 * uint32_t_size;         // Event record header

            uint32_t eventRecordNameLength = ReadUint32(readBuffer,
                                                        eventRecordPoolOffset + eventRecord.name_offset);
            eventRecord.name_length = eventRecordNameLength; // name_length
            unsigned char eventRecordNameNullTerminator =
                    ReadUint8(readBuffer,
                              eventRecordPoolOffset +
                              eventRecord.name_offset +
                              uint32_t_size +
                              eventRecordNameLength - 1); // name null-terminator
            BOOST_CHECK(eventRecordNameNullTerminator == '\0');
            std::vector<unsigned char> eventRecordNameBuffer(eventRecord.name_length - 1);
            std::memcpy(eventRecordNameBuffer.data(),
                        readData +
                        eventRecordPoolOffset +
                        eventRecord.name_offset +
                        uint32_t_size,
                        eventRecordNameBuffer.size());
            eventRecord.name.assign(eventRecordNameBuffer.begin(), eventRecordNameBuffer.end()); // name

            uint32_t eventRecordDescriptionLength = ReadUint32(readBuffer,
                                                               eventRecordPoolOffset + eventRecord.description_offset);
            eventRecord.description_length = eventRecordDescriptionLength; // description_length
            unsigned char eventRecordDescriptionNullTerminator =
                    ReadUint8(readBuffer,
                              eventRecordPoolOffset +
                              eventRecord.description_offset +
                              uint32_t_size +
                              eventRecordDescriptionLength - 1); // description null-terminator
            BOOST_CHECK(eventRecordDescriptionNullTerminator == '\0');
            std::vector<unsigned char> eventRecordDescriptionBuffer(eventRecord.description_length - 1);
            std::memcpy(eventRecordDescriptionBuffer.data(),
                        readData +
                        eventRecordPoolOffset +
                        eventRecord.description_offset +
                        uint32_t_size,
                        eventRecordDescriptionBuffer.size());
            eventRecord.description.assign(eventRecordDescriptionBuffer.begin(),
                                           eventRecordDescriptionBuffer.end()); // description

            if (eventRecord.units_offset > 0)
            {
                uint32_t eventRecordUnitsLength = ReadUint32(readBuffer,
                                                             eventRecordPoolOffset + eventRecord.units_offset);
                eventRecord.units_length = eventRecordUnitsLength; // units_length
                unsigned char eventRecordUnitsNullTerminator =
                        ReadUint8(readBuffer,
                                  eventRecordPoolOffset +
                                  eventRecord.units_offset +
                                  uint32_t_size +
                                  eventRecordUnitsLength - 1); // units null-terminator
                BOOST_CHECK(eventRecordUnitsNullTerminator == '\0');
                std::vector<unsigned char> eventRecordUnitsBuffer(eventRecord.units_length - 1);
                std::memcpy(eventRecordUnitsBuffer.data(),
                            readData +
                            eventRecordPoolOffset +
                            eventRecord.units_offset +
                            uint32_t_size,
                            eventRecordUnitsBuffer.size());
                eventRecord.units.assign(eventRecordUnitsBuffer.begin(), eventRecordUnitsBuffer.end()); // units
            }

            categoryRecord.event_records.push_back(eventRecord);
        }

        categoryRecords.push_back(categoryRecord);
    }

    // Check that the category records are correct
    BOOST_CHECK(categoryRecords.size() == 2);
    for (const CategoryRecord& categoryRecord : categoryRecords)
    {
        const Category* category = counterDirectory.GetCategory(categoryRecord.name);
        BOOST_CHECK(category);
        BOOST_CHECK(category->m_Name == categoryRecord.name);
        BOOST_CHECK(category->m_DeviceUid == categoryRecord.device);
        BOOST_CHECK(category->m_CounterSetUid == categoryRecord.counter_set);
        BOOST_CHECK(category->m_Counters.size() == categoryRecord.event_count);

        // Check that the event records are correct
        for (const EventRecord& eventRecord : categoryRecord.event_records)
        {
            const Counter* counter = counterDirectory.GetCounter(eventRecord.counter_uid);
            BOOST_CHECK(counter);
            BOOST_CHECK(counter->m_MaxCounterUid == eventRecord.max_counter_uid);
            BOOST_CHECK(counter->m_DeviceUid == eventRecord.device);
            BOOST_CHECK(counter->m_CounterSetUid == eventRecord.counter_set);
            BOOST_CHECK(counter->m_Class == eventRecord.counter_class);
            BOOST_CHECK(counter->m_Interpolation == eventRecord.interpolation);
            BOOST_CHECK(counter->m_Multiplier == eventRecord.multiplier);
            BOOST_CHECK(counter->m_Name == eventRecord.name);
            BOOST_CHECK(counter->m_Description == eventRecord.description);
            BOOST_CHECK(counter->m_Units == eventRecord.units);
        }
    }
}

BOOST_AUTO_TEST_CASE(SendCounterDirectoryPacketTest3)
{
    ProfilingStateMachine profilingStateMachine;

    // Using a mock counter directory that allows to register invalid objects
    MockCounterDirectory counterDirectory;

    // Register an invalid device
    const std::string deviceName = "inv@lid dev!c€";
    const Device* device = nullptr;
    BOOST_CHECK_NO_THROW(device = counterDirectory.RegisterDevice(deviceName, 3));
    BOOST_CHECK(counterDirectory.GetDeviceCount() == 1);
    BOOST_CHECK(device);

    // Buffer with enough space
    MockBufferManager mockBuffer(1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, mockBuffer);
    BOOST_CHECK_THROW(sendCounterPacket.SendCounterDirectoryPacket(counterDirectory), armnn::RuntimeException);
}

BOOST_AUTO_TEST_CASE(SendCounterDirectoryPacketTest4)
{
    ProfilingStateMachine profilingStateMachine;

    // Using a mock counter directory that allows to register invalid objects
    MockCounterDirectory counterDirectory;

    // Register an invalid counter set
    const std::string counterSetName = "inv@lid count€rs€t";
    const CounterSet* counterSet = nullptr;
    BOOST_CHECK_NO_THROW(counterSet = counterDirectory.RegisterCounterSet(counterSetName));
    BOOST_CHECK(counterDirectory.GetCounterSetCount() == 1);
    BOOST_CHECK(counterSet);

    // Buffer with enough space
    MockBufferManager mockBuffer(1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, mockBuffer);
    BOOST_CHECK_THROW(sendCounterPacket.SendCounterDirectoryPacket(counterDirectory), armnn::RuntimeException);
}

BOOST_AUTO_TEST_CASE(SendCounterDirectoryPacketTest5)
{
    ProfilingStateMachine profilingStateMachine;

    // Using a mock counter directory that allows to register invalid objects
    MockCounterDirectory counterDirectory;

    // Register an invalid category
    const std::string categoryName = "c@t€gory";
    const Category* category = nullptr;
    BOOST_CHECK_NO_THROW(category = counterDirectory.RegisterCategory(categoryName));
    BOOST_CHECK(counterDirectory.GetCategoryCount() == 1);
    BOOST_CHECK(category);

    // Buffer with enough space
    MockBufferManager mockBuffer(1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, mockBuffer);
    BOOST_CHECK_THROW(sendCounterPacket.SendCounterDirectoryPacket(counterDirectory), armnn::RuntimeException);
}

BOOST_AUTO_TEST_CASE(SendCounterDirectoryPacketTest6)
{
    ProfilingStateMachine profilingStateMachine;

    // Using a mock counter directory that allows to register invalid objects
    MockCounterDirectory counterDirectory;

    // Register an invalid device
    const std::string deviceName = "inv@lid dev!c€";
    const Device* device = nullptr;
    BOOST_CHECK_NO_THROW(device = counterDirectory.RegisterDevice(deviceName, 3));
    BOOST_CHECK(counterDirectory.GetDeviceCount() == 1);
    BOOST_CHECK(device);

    // Register an invalid counter set
    const std::string counterSetName = "inv@lid count€rs€t";
    const CounterSet* counterSet = nullptr;
    BOOST_CHECK_NO_THROW(counterSet = counterDirectory.RegisterCounterSet(counterSetName));
    BOOST_CHECK(counterDirectory.GetCounterSetCount() == 1);
    BOOST_CHECK(counterSet);

    // Register an invalid category associated to an invalid device and an invalid counter set
    const std::string categoryName = "c@t€gory";
    const Category* category = nullptr;
    BOOST_CHECK_NO_THROW(category = counterDirectory.RegisterCategory(categoryName,
                                                                      device->m_Uid,
                                                                      counterSet->m_Uid));
    BOOST_CHECK(counterDirectory.GetCategoryCount() == 1);
    BOOST_CHECK(category);

    // Buffer with enough space
    MockBufferManager mockBuffer(1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, mockBuffer);
    BOOST_CHECK_THROW(sendCounterPacket.SendCounterDirectoryPacket(counterDirectory), armnn::RuntimeException);
}

BOOST_AUTO_TEST_CASE(SendCounterDirectoryPacketTest7)
{
    ProfilingStateMachine profilingStateMachine;

    // Using a mock counter directory that allows to register invalid objects
    MockCounterDirectory counterDirectory;

    // Register an valid device
    const std::string deviceName = "valid device";
    const Device* device = nullptr;
    BOOST_CHECK_NO_THROW(device = counterDirectory.RegisterDevice(deviceName, 3));
    BOOST_CHECK(counterDirectory.GetDeviceCount() == 1);
    BOOST_CHECK(device);

    // Register an valid counter set
    const std::string counterSetName = "valid counterset";
    const CounterSet* counterSet = nullptr;
    BOOST_CHECK_NO_THROW(counterSet = counterDirectory.RegisterCounterSet(counterSetName));
    BOOST_CHECK(counterDirectory.GetCounterSetCount() == 1);
    BOOST_CHECK(counterSet);

    // Register an valid category associated to a valid device and a valid counter set
    const std::string categoryName = "category";
    const Category* category = nullptr;
    BOOST_CHECK_NO_THROW(category = counterDirectory.RegisterCategory(categoryName,
                                                                      device->m_Uid,
                                                                      counterSet->m_Uid));
    BOOST_CHECK(counterDirectory.GetCategoryCount() == 1);
    BOOST_CHECK(category);

    // Register an invalid counter associated to a valid category
    const Counter* counter = nullptr;
    BOOST_CHECK_NO_THROW(counter = counterDirectory.RegisterCounter(categoryName,
                                                                    0,
                                                                    1,
                                                                    123.45f,
                                                                    "counter",
                                                                    "counter description",
                                                                    std::string("invalid counter units"),
                                                                    5,
                                                                    device->m_Uid,
                                                                    counterSet->m_Uid));
    BOOST_CHECK(counterDirectory.GetCounterCount() == 5);
    BOOST_CHECK(counter);

    // Buffer with enough space
    MockBufferManager mockBuffer(1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, mockBuffer);
    BOOST_CHECK_THROW(sendCounterPacket.SendCounterDirectoryPacket(counterDirectory), armnn::RuntimeException);
}

BOOST_AUTO_TEST_CASE(SendThreadTest0)
{
    ProfilingStateMachine profilingStateMachine;
    SetActiveProfilingState(profilingStateMachine);

    MockProfilingConnection mockProfilingConnection;
    MockStreamCounterBuffer mockStreamCounterBuffer(0);
    SendCounterPacket sendCounterPacket(profilingStateMachine, mockStreamCounterBuffer);

    // Try to start the send thread many times, it must only start once

    sendCounterPacket.Start(mockProfilingConnection);
    BOOST_CHECK(sendCounterPacket.IsRunning());
    sendCounterPacket.Start(mockProfilingConnection);
    sendCounterPacket.Start(mockProfilingConnection);
    sendCounterPacket.Start(mockProfilingConnection);
    sendCounterPacket.Start(mockProfilingConnection);
    BOOST_CHECK(sendCounterPacket.IsRunning());

    std::this_thread::sleep_for(std::chrono::seconds(1));

    sendCounterPacket.Stop();
    BOOST_CHECK(!sendCounterPacket.IsRunning());
}

BOOST_AUTO_TEST_CASE(SendThreadTest1)
{
    ProfilingStateMachine profilingStateMachine;
    SetActiveProfilingState(profilingStateMachine);

    unsigned int totalWrittenSize = 0;

    MockProfilingConnection mockProfilingConnection;
    MockStreamCounterBuffer mockStreamCounterBuffer(1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, mockStreamCounterBuffer);
    sendCounterPacket.Start(mockProfilingConnection);

    // Interleaving writes and reads to/from the buffer with pauses to test that the send thread actually waits for
    // something to become available for reading

    std::this_thread::sleep_for(std::chrono::seconds(1));

    CounterDirectory counterDirectory;
    sendCounterPacket.SendStreamMetaDataPacket();

    // Get the size of the Stream Metadata Packet
    std::string processName = GetProcessName().substr(0, 60);
    unsigned int processNameSize = processName.empty() ? 0 : boost::numeric_cast<unsigned int>(processName.size()) + 1;
    unsigned int streamMetadataPacketsize = 118 + processNameSize;
    totalWrittenSize += streamMetadataPacketsize;

    sendCounterPacket.SetReadyToRead();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    sendCounterPacket.SendCounterDirectoryPacket(counterDirectory);

    // Get the size of the Counter Directory Packet
    unsigned int counterDirectoryPacketSize = 32;
    totalWrittenSize += counterDirectoryPacketSize;

    sendCounterPacket.SetReadyToRead();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    sendCounterPacket.SendPeriodicCounterCapturePacket(123u,
                                                       {
                                                           {   1u,      23u },
                                                           {  33u, 1207623u }
                                                       });

    // Get the size of the Periodic Counter Capture Packet
    unsigned int periodicCounterCapturePacketSize = 28;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SetReadyToRead();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    sendCounterPacket.SendPeriodicCounterCapturePacket(44u,
                                                       {
                                                           { 211u,     923u }
                                                       });

    // Get the size of the Periodic Counter Capture Packet
    periodicCounterCapturePacketSize = 22;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SendPeriodicCounterCapturePacket(1234u,
                                                       {
                                                           { 555u,      23u },
                                                           { 556u,       6u },
                                                           { 557u,  893454u },
                                                           { 558u, 1456623u },
                                                           { 559u,  571090u }
                                                       });

    // Get the size of the Periodic Counter Capture Packet
    periodicCounterCapturePacketSize = 46;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SendPeriodicCounterCapturePacket(997u,
                                                       {
                                                           {  88u,      11u },
                                                           {  96u,      22u },
                                                           {  97u,      33u },
                                                           { 999u,     444u }
                                                       });

    // Get the size of the Periodic Counter Capture Packet
    periodicCounterCapturePacketSize = 40;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SetReadyToRead();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    sendCounterPacket.SendPeriodicCounterSelectionPacket(1000u, { 1345u, 254u, 4536u, 408u, 54u, 6323u, 428u, 1u, 6u });

    // Get the size of the Periodic Counter Capture Packet
    periodicCounterCapturePacketSize = 30;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SetReadyToRead();

    // To test an exact value of the "read size" in the mock buffer, wait two seconds to allow the send thread to
    // read all what's remaining in the buffer
    std::this_thread::sleep_for(std::chrono::seconds(2));

    sendCounterPacket.Stop();

    BOOST_CHECK(mockStreamCounterBuffer.GetCommittedSize() == totalWrittenSize);
    BOOST_CHECK(mockStreamCounterBuffer.GetReadableSize()  == totalWrittenSize);
    BOOST_CHECK(mockStreamCounterBuffer.GetReadSize()      == totalWrittenSize);
}

BOOST_AUTO_TEST_CASE(SendThreadTest2)
{
    ProfilingStateMachine profilingStateMachine;
    SetActiveProfilingState(profilingStateMachine);

    unsigned int totalWrittenSize = 0;

    MockProfilingConnection mockProfilingConnection;
    MockStreamCounterBuffer mockStreamCounterBuffer(1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, mockStreamCounterBuffer);
    sendCounterPacket.Start(mockProfilingConnection);

    // Adding many spurious "ready to read" signals throughout the test to check that the send thread is
    // capable of handling unnecessary read requests

    std::this_thread::sleep_for(std::chrono::seconds(1));

    sendCounterPacket.SetReadyToRead();

    CounterDirectory counterDirectory;
    sendCounterPacket.SendStreamMetaDataPacket();

    // Get the size of the Stream Metadata Packet
    std::string processName = GetProcessName().substr(0, 60);
    unsigned int processNameSize = processName.empty() ? 0 : boost::numeric_cast<unsigned int>(processName.size()) + 1;
    unsigned int streamMetadataPacketsize = 118 + processNameSize;
    totalWrittenSize += streamMetadataPacketsize;

    sendCounterPacket.SetReadyToRead();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    sendCounterPacket.SendCounterDirectoryPacket(counterDirectory);

    // Get the size of the Counter Directory Packet
    unsigned int counterDirectoryPacketSize = 32;
    totalWrittenSize += counterDirectoryPacketSize;

    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SetReadyToRead();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    sendCounterPacket.SendPeriodicCounterCapturePacket(123u,
                                                       {
                                                           {   1u,      23u },
                                                           {  33u, 1207623u }
                                                       });

    // Get the size of the Periodic Counter Capture Packet
    unsigned int periodicCounterCapturePacketSize = 28;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SetReadyToRead();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SetReadyToRead();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SendPeriodicCounterCapturePacket(44u,
                                                       {
                                                           { 211u,     923u }
                                                       });

    // Get the size of the Periodic Counter Capture Packet
    periodicCounterCapturePacketSize = 22;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SendPeriodicCounterCapturePacket(1234u,
                                                       {
                                                           { 555u,      23u },
                                                           { 556u,       6u },
                                                           { 557u,  893454u },
                                                           { 558u, 1456623u },
                                                           { 559u,  571090u }
                                                       });

    // Get the size of the Periodic Counter Capture Packet
    periodicCounterCapturePacketSize = 46;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SendPeriodicCounterCapturePacket(997u,
                                                       {
                                                           {  88u,      11u },
                                                           {  96u,      22u },
                                                           {  97u,      33u },
                                                           { 999u,     444u }
                                                       });

    // Get the size of the Periodic Counter Capture Packet
    periodicCounterCapturePacketSize = 40;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SetReadyToRead();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    sendCounterPacket.SendPeriodicCounterSelectionPacket(1000u, { 1345u, 254u, 4536u, 408u, 54u, 6323u, 428u, 1u, 6u });

    // Get the size of the Periodic Counter Capture Packet
    periodicCounterCapturePacketSize = 30;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SetReadyToRead();

    // To test an exact value of the "read size" in the mock buffer, wait two seconds to allow the send thread to
    // read all what's remaining in the buffer
    std::this_thread::sleep_for(std::chrono::seconds(2));

    sendCounterPacket.Stop();

    BOOST_CHECK(mockStreamCounterBuffer.GetCommittedSize() == totalWrittenSize);
    BOOST_CHECK(mockStreamCounterBuffer.GetReadableSize()  == totalWrittenSize);
    BOOST_CHECK(mockStreamCounterBuffer.GetReadSize()      == totalWrittenSize);
}

BOOST_AUTO_TEST_CASE(SendThreadTest3)
{
    ProfilingStateMachine profilingStateMachine;
    SetActiveProfilingState(profilingStateMachine);

    unsigned int totalWrittenSize = 0;

    MockProfilingConnection mockProfilingConnection;
    MockStreamCounterBuffer mockStreamCounterBuffer(1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, mockStreamCounterBuffer);
    sendCounterPacket.Start(mockProfilingConnection);

    // Not using pauses or "grace periods" to stress test the send thread

    sendCounterPacket.SetReadyToRead();

    CounterDirectory counterDirectory;
    sendCounterPacket.SendStreamMetaDataPacket();

    // Get the size of the Stream Metadata Packet
    std::string processName = GetProcessName().substr(0, 60);
    unsigned int processNameSize = processName.empty() ? 0 : boost::numeric_cast<unsigned int>(processName.size()) + 1;
    unsigned int streamMetadataPacketsize = 118 + processNameSize;
    totalWrittenSize += streamMetadataPacketsize;

    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SendCounterDirectoryPacket(counterDirectory);

    // Get the size of the Counter Directory Packet
    unsigned int counterDirectoryPacketSize =32;
    totalWrittenSize += counterDirectoryPacketSize;

    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SendPeriodicCounterCapturePacket(123u,
                                                       {
                                                           {   1u,      23u },
                                                           {  33u, 1207623u }
                                                       });

    // Get the size of the Periodic Counter Capture Packet
    unsigned int periodicCounterCapturePacketSize = 28;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SendPeriodicCounterCapturePacket(44u,
                                                       {
                                                           { 211u,     923u }
                                                       });

    // Get the size of the Periodic Counter Capture Packet
    periodicCounterCapturePacketSize = 22;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SendPeriodicCounterCapturePacket(1234u,
                                                       {
                                                           { 555u,      23u },
                                                           { 556u,       6u },
                                                           { 557u,  893454u },
                                                           { 558u, 1456623u },
                                                           { 559u,  571090u }
                                                       });

    // Get the size of the Periodic Counter Capture Packet
    periodicCounterCapturePacketSize = 46;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SendPeriodicCounterCapturePacket(997u,
                                                       {
                                                           {  88u,      11u },
                                                           {  96u,      22u },
                                                           {  97u,      33u },
                                                           { 999u,     444u }
                                                       });

    // Get the size of the Periodic Counter Capture Packet
    periodicCounterCapturePacketSize = 40;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SetReadyToRead();
    sendCounterPacket.SendPeriodicCounterSelectionPacket(1000u, { 1345u, 254u, 4536u, 408u, 54u, 6323u, 428u, 1u, 6u });

    // Get the size of the Periodic Counter Capture Packet
    periodicCounterCapturePacketSize = 30;
    totalWrittenSize += periodicCounterCapturePacketSize;

    sendCounterPacket.SetReadyToRead();

    // Abruptly terminating the send thread, the amount of data sent may be less that the amount written (the send
    // thread is not guaranteed to flush the buffer)
    sendCounterPacket.Stop();

    BOOST_CHECK(mockStreamCounterBuffer.GetCommittedSize() == totalWrittenSize);
    BOOST_CHECK(mockStreamCounterBuffer.GetReadableSize()  <= totalWrittenSize);
    BOOST_CHECK(mockStreamCounterBuffer.GetReadSize()      <= totalWrittenSize);
    BOOST_CHECK(mockStreamCounterBuffer.GetReadSize()      <= mockStreamCounterBuffer.GetReadableSize());
    BOOST_CHECK(mockStreamCounterBuffer.GetReadSize()      <= mockStreamCounterBuffer.GetCommittedSize());
}

BOOST_AUTO_TEST_CASE(SendThreadBufferTest)
{
    ProfilingStateMachine profilingStateMachine;
    SetActiveProfilingState(profilingStateMachine);

    MockProfilingConnection mockProfilingConnection;
    BufferManager bufferManager(1, 1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, bufferManager, -1);
    sendCounterPacket.Start(mockProfilingConnection);

    // Interleaving writes and reads to/from the buffer with pauses to test that the send thread actually waits for
    // something to become available for reading
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // SendStreamMetaDataPacket
    sendCounterPacket.SendStreamMetaDataPacket();

    // Read data from the buffer
    // Buffer should become readable after commit by SendStreamMetaDataPacket
    auto packetBuffer = bufferManager.GetReadableBuffer();
    BOOST_TEST(packetBuffer.get());

    std::string processName = GetProcessName().substr(0, 60);
    unsigned int processNameSize = processName.empty() ? 0 : boost::numeric_cast<unsigned int>(processName.size()) + 1;
    unsigned int streamMetadataPacketsize = 118 + processNameSize;
    BOOST_TEST(packetBuffer->GetSize() == streamMetadataPacketsize);

    // Buffer is not available when SendStreamMetaDataPacket already occupied the buffer.
    unsigned int reservedSize = 0;
    auto reservedBuffer = bufferManager.Reserve(512, reservedSize);
    BOOST_TEST(!reservedBuffer.get());

    // Recommit to be read by sendCounterPacket
    bufferManager.Commit(packetBuffer, streamMetadataPacketsize);

    sendCounterPacket.SetReadyToRead();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // The buffer is read by the send thread so it should not be in the readable buffer.
    auto readBuffer = bufferManager.GetReadableBuffer();
    BOOST_TEST(!readBuffer);

    // Successfully reserved the buffer with requested size
    reservedBuffer = bufferManager.Reserve(512, reservedSize);
    BOOST_TEST(reservedSize == 512);
    BOOST_TEST(reservedBuffer.get());

    // Release the buffer to be used by sendCounterPacket
    bufferManager.Release(reservedBuffer);

    // SendCounterDirectoryPacket
    CounterDirectory counterDirectory;
    sendCounterPacket.SendCounterDirectoryPacket(counterDirectory);

    // Read data from the buffer
    // Buffer should become readable after commit by SendCounterDirectoryPacket
    auto counterDirectoryPacketBuffer = bufferManager.GetReadableBuffer();
    BOOST_TEST(counterDirectoryPacketBuffer.get());

    // Get the size of the Counter Directory Packet
    unsigned int counterDirectoryPacketSize = 32;
    BOOST_TEST(counterDirectoryPacketBuffer->GetSize() == counterDirectoryPacketSize);

    // Buffer is not available when SendCounterDirectoryPacket already occupied the buffer.
    reservedSize = 0;
    reservedBuffer = bufferManager.Reserve(512, reservedSize);
    BOOST_TEST(reservedSize == 0);
    BOOST_TEST(!reservedBuffer.get());

    // Recommit to be read by sendCounterPacket
    bufferManager.Commit(counterDirectoryPacketBuffer, counterDirectoryPacketSize);

    sendCounterPacket.SetReadyToRead();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // The buffer is read by the send thread so it should not be in the readable buffer.
    readBuffer = bufferManager.GetReadableBuffer();
    BOOST_TEST(!readBuffer);

    // Successfully reserved the buffer with requested size
    reservedBuffer = bufferManager.Reserve(512, reservedSize);
    BOOST_TEST(reservedSize == 512);
    BOOST_TEST(reservedBuffer.get());

    // Release the buffer to be used by sendCounterPacket
    bufferManager.Release(reservedBuffer);

    // SendPeriodicCounterCapturePacket
    sendCounterPacket.SendPeriodicCounterCapturePacket(123u,
                                                       {
                                                           {   1u,      23u },
                                                           {  33u, 1207623u }
                                                       });

    // Read data from the buffer
    // Buffer should become readable after commit by SendPeriodicCounterCapturePacket
    auto periodicCounterCapturePacketBuffer = bufferManager.GetReadableBuffer();
    BOOST_TEST(periodicCounterCapturePacketBuffer.get());

    // Get the size of the Periodic Counter Capture Packet
    unsigned int periodicCounterCapturePacketSize = 28;
    BOOST_TEST(periodicCounterCapturePacketBuffer->GetSize() == periodicCounterCapturePacketSize);

    // Buffer is not available when SendPeriodicCounterCapturePacket already occupied the buffer.
    reservedSize = 0;
    reservedBuffer = bufferManager.Reserve(512, reservedSize);
    BOOST_TEST(reservedSize == 0);
    BOOST_TEST(!reservedBuffer.get());

    // Recommit to be read by sendCounterPacket
    bufferManager.Commit(periodicCounterCapturePacketBuffer, periodicCounterCapturePacketSize);

    sendCounterPacket.SetReadyToRead();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // The buffer is read by the send thread so it should not be in the readable buffer.
    readBuffer = bufferManager.GetReadableBuffer();
    BOOST_TEST(!readBuffer);

    // Successfully reserved the buffer with requested size
    reservedBuffer = bufferManager.Reserve(512, reservedSize);
    BOOST_TEST(reservedSize == 512);
    BOOST_TEST(reservedBuffer.get());

    sendCounterPacket.Stop();
}

BOOST_AUTO_TEST_CASE(SendThreadBufferTest1)
{
    ProfilingStateMachine profilingStateMachine;
    SetActiveProfilingState(profilingStateMachine);

    MockProfilingConnection mockProfilingConnection;
    BufferManager bufferManager(3, 1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, bufferManager, -1);
    sendCounterPacket.Start(mockProfilingConnection);

    // SendStreamMetaDataPacket
    sendCounterPacket.SendStreamMetaDataPacket();

    // Read data from the buffer
    // Buffer should become readable after commit by SendStreamMetaDataPacket
    auto packetBuffer = bufferManager.GetReadableBuffer();
    BOOST_TEST(packetBuffer.get());

    std::string processName = GetProcessName().substr(0, 60);
    unsigned int processNameSize = processName.empty() ? 0 : boost::numeric_cast<unsigned int>(processName.size()) + 1;
    unsigned int streamMetadataPacketsize = 118 + processNameSize;
    BOOST_TEST(packetBuffer->GetSize() == streamMetadataPacketsize);

    // Recommit to be read by sendCounterPacket
    bufferManager.Commit(packetBuffer, streamMetadataPacketsize);

    sendCounterPacket.SetReadyToRead();

    // SendCounterDirectoryPacket
    CounterDirectory counterDirectory;
    sendCounterPacket.SendCounterDirectoryPacket(counterDirectory);

    sendCounterPacket.SetReadyToRead();

    // SendPeriodicCounterCapturePacket
    sendCounterPacket.SendPeriodicCounterCapturePacket(123u,
                                                       {
                                                           {   1u,      23u },
                                                           {  33u, 1207623u }
                                                       });

    sendCounterPacket.SetReadyToRead();

    sendCounterPacket.Stop();

    // The buffer is read by the send thread so it should not be in the readable buffer.
    auto readBuffer = bufferManager.GetReadableBuffer();
    BOOST_TEST(!readBuffer);

    // Successfully reserved the buffer with requested size
    unsigned int reservedSize = 0;
    auto reservedBuffer = bufferManager.Reserve(512, reservedSize);
    BOOST_TEST(reservedSize == 512);
    BOOST_TEST(reservedBuffer.get());

    // Check that data was actually written to the profiling connection in any order
    const std::vector<uint32_t> writtenData = mockProfilingConnection.GetWrittenData();
    BOOST_TEST(writtenData.size() == 3);
    bool foundStreamMetaDataPacket =
        std::find(writtenData.begin(), writtenData.end(), streamMetadataPacketsize) != writtenData.end();
    bool foundCounterDirectoryPacket = std::find(writtenData.begin(), writtenData.end(), 32) != writtenData.end();
    bool foundPeriodicCounterCapturePacket = std::find(writtenData.begin(), writtenData.end(), 28) != writtenData.end();
    BOOST_TEST(foundStreamMetaDataPacket);
    BOOST_TEST(foundCounterDirectoryPacket);
    BOOST_TEST(foundPeriodicCounterCapturePacket);
}

BOOST_AUTO_TEST_CASE(SendThreadSendStreamMetadataPacket1)
{
    ProfilingStateMachine profilingStateMachine;

    MockProfilingConnection mockProfilingConnection;
    BufferManager bufferManager(3, 1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, bufferManager);
    sendCounterPacket.Start(mockProfilingConnection);

    // The profiling state is set to "Uninitialized", so the send thread should throw an exception

    // Wait a bit to make sure that the send thread is properly started
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    BOOST_CHECK_THROW(sendCounterPacket.Stop(), armnn::RuntimeException);
}

BOOST_AUTO_TEST_CASE(SendThreadSendStreamMetadataPacket2)
{
    ProfilingStateMachine profilingStateMachine;
    SetNotConnectedProfilingState(profilingStateMachine);

    MockProfilingConnection mockProfilingConnection;
    BufferManager bufferManager(3, 1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, bufferManager);
    sendCounterPacket.Start(mockProfilingConnection);

    // The profiling state is set to "NotConnected", so the send thread should throw an exception

    // Wait a bit to make sure that the send thread is properly started
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    BOOST_CHECK_THROW(sendCounterPacket.Stop(), armnn::RuntimeException);
}

BOOST_AUTO_TEST_CASE(SendThreadSendStreamMetadataPacket3)
{
    ProfilingStateMachine profilingStateMachine;
    SetWaitingForAckProfilingState(profilingStateMachine);

    // Calculate the size of a Stream Metadata packet
    std::string processName = GetProcessName().substr(0, 60);
    unsigned int processNameSize = processName.empty() ? 0 : boost::numeric_cast<unsigned int>(processName.size()) + 1;
    unsigned int streamMetadataPacketsize = 118 + processNameSize;

    MockProfilingConnection mockProfilingConnection;
    BufferManager bufferManager(3, 1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, bufferManager);
    sendCounterPacket.Start(mockProfilingConnection);

    // The profiling state is set to "WaitingForAck", so the send thread should send a Stream Metadata packet

    // Wait for a bit to make sure that we get the packet
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    BOOST_CHECK_NO_THROW(sendCounterPacket.Stop());

    // Check that the buffer contains one Stream Metadata packet
    const std::vector<uint32_t> writtenData = mockProfilingConnection.GetWrittenData();
    BOOST_TEST(writtenData.size() == 1);
    BOOST_TEST(writtenData[0] == streamMetadataPacketsize);
}

BOOST_AUTO_TEST_CASE(SendThreadSendStreamMetadataPacket4)
{
    ProfilingStateMachine profilingStateMachine;
    SetWaitingForAckProfilingState(profilingStateMachine);

    // Calculate the size of a Stream Metadata packet
    std::string processName = GetProcessName().substr(0, 60);
    unsigned int processNameSize = processName.empty() ? 0 : boost::numeric_cast<unsigned int>(processName.size()) + 1;
    unsigned int streamMetadataPacketsize = 118 + processNameSize;

    MockProfilingConnection mockProfilingConnection;
    BufferManager bufferManager(3, 1024);
    SendCounterPacket sendCounterPacket(profilingStateMachine, bufferManager);
    sendCounterPacket.Start(mockProfilingConnection);

    // The profiling state is set to "WaitingForAck", so the send thread should send a Stream Metadata packet

    // Wait for a bit to make sure that we get the packet
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check that the profiling state is still "WaitingForAck"
    BOOST_TEST((profilingStateMachine.GetCurrentState() == ProfilingState::WaitingForAck));

    // Check that the buffer contains one Stream Metadata packet
    const std::vector<uint32_t> writtenData = mockProfilingConnection.GetWrittenData();
    BOOST_TEST(writtenData.size() == 1);
    BOOST_TEST(writtenData[0] == streamMetadataPacketsize);

    mockProfilingConnection.Clear();

    // Try triggering a new buffer read
    sendCounterPacket.SetReadyToRead();

    // Wait for a bit to make sure that we get the packet
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check that the profiling state is still "WaitingForAck"
    BOOST_TEST((profilingStateMachine.GetCurrentState() == ProfilingState::WaitingForAck));

    // Check that the buffer contains one Stream Metadata packet
    BOOST_TEST(writtenData.size() == 1);
    BOOST_TEST(writtenData[0] == streamMetadataPacketsize);

    BOOST_CHECK_NO_THROW(sendCounterPacket.Stop());
}

BOOST_AUTO_TEST_SUITE_END()
