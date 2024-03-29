//
// Copyright © 2019 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "SendCounterPacketTests.hpp"

#include <CommandHandlerFunctor.hpp>
#include <IProfilingConnection.hpp>
#include <IProfilingConnectionFactory.hpp>
#include <Logging.hpp>
#include <ProfilingService.hpp>

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <iostream>
#include <thread>

namespace armnn
{

namespace profiling
{

struct LogLevelSwapper
{
public:
    LogLevelSwapper(armnn::LogSeverity severity)
    {
        // Set the new log level
        armnn::ConfigureLogging(true, true, severity);
    }
    ~LogLevelSwapper()
    {
        // The default log level for unit tests is "Fatal"
        armnn::ConfigureLogging(true, true, armnn::LogSeverity::Fatal);
    }
};

struct StreamRedirector
{
public:
    StreamRedirector(std::ostream& stream, std::streambuf* newStreamBuffer)
        : m_Stream(stream)
        , m_BackupBuffer(m_Stream.rdbuf(newStreamBuffer))
    {}
    ~StreamRedirector() { m_Stream.rdbuf(m_BackupBuffer); }

private:
    std::ostream& m_Stream;
    std::streambuf* m_BackupBuffer;
};

class TestProfilingConnectionBase : public IProfilingConnection
{
public:
    TestProfilingConnectionBase() = default;
    ~TestProfilingConnectionBase() = default;

    bool IsOpen() const override { return true; }

    void Close() override {}

    bool WritePacket(const unsigned char* buffer, uint32_t length) override { return false; }

    Packet ReadPacket(uint32_t timeout) override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));

        // Return connection acknowledged packet
        return Packet(65536);
    }
};

class TestProfilingConnectionTimeoutError : public TestProfilingConnectionBase
{
public:
    TestProfilingConnectionTimeoutError()
        : m_ReadRequests(0)
    {}

    Packet ReadPacket(uint32_t timeout) override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));

        if (m_ReadRequests < 3)
        {
            m_ReadRequests++;
            throw armnn::TimeoutException("Simulate a timeout error\n");
        }

        // Return connection acknowledged packet after three timeouts
        return Packet(65536);
    }

private:
    int m_ReadRequests;
};

class TestProfilingConnectionArmnnError : public TestProfilingConnectionBase
{
public:
    Packet ReadPacket(uint32_t timeout) override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));

        throw armnn::Exception("Simulate a non-timeout error");
    }
};

class TestFunctorA : public CommandHandlerFunctor
{
public:
    using CommandHandlerFunctor::CommandHandlerFunctor;

    int GetCount() { return m_Count; }

    void operator()(const Packet& packet) override
    {
        m_Count++;
    }

private:
    int m_Count = 0;
};

class TestFunctorB : public TestFunctorA
{
    using TestFunctorA::TestFunctorA;
};

class TestFunctorC : public TestFunctorA
{
    using TestFunctorA::TestFunctorA;
};

class MockProfilingConnectionFactory : public IProfilingConnectionFactory
{
public:
    IProfilingConnectionPtr GetProfilingConnection(const ExternalProfilingOptions& options) const override
    {
        return std::make_unique<MockProfilingConnection>();
    }
};

class SwapProfilingConnectionFactoryHelper : public ProfilingService
{
public:
    using MockProfilingConnectionFactoryPtr = std::unique_ptr<MockProfilingConnectionFactory>;

    SwapProfilingConnectionFactoryHelper()
        : ProfilingService()
        , m_MockProfilingConnectionFactory(new MockProfilingConnectionFactory())
        , m_BackupProfilingConnectionFactory(nullptr)
    {
        BOOST_CHECK(m_MockProfilingConnectionFactory);
        SwapProfilingConnectionFactory(ProfilingService::Instance(),
                                       m_MockProfilingConnectionFactory.get(),
                                       m_BackupProfilingConnectionFactory);
        BOOST_CHECK(m_BackupProfilingConnectionFactory);
    }
    ~SwapProfilingConnectionFactoryHelper()
    {
        BOOST_CHECK(m_BackupProfilingConnectionFactory);
        IProfilingConnectionFactory* temp = nullptr;
        SwapProfilingConnectionFactory(ProfilingService::Instance(),
                                       m_BackupProfilingConnectionFactory,
                                       temp);
    }

    MockProfilingConnection* GetMockProfilingConnection()
    {
        IProfilingConnection* profilingConnection = GetProfilingConnection(ProfilingService::Instance());
        return boost::polymorphic_downcast<MockProfilingConnection*>(profilingConnection);
    }

    void ForceTransitionToState(ProfilingState newState)
    {
        TransitionToState(ProfilingService::Instance(), newState);
    }

private:
    MockProfilingConnectionFactoryPtr m_MockProfilingConnectionFactory;
    IProfilingConnectionFactory* m_BackupProfilingConnectionFactory;
};

} // namespace profiling

} // namespace armnn
