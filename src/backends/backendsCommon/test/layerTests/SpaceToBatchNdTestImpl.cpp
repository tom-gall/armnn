//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "SpaceToBatchNdTestImpl.hpp"

#include <Permute.hpp>
#include <ResolveType.hpp>

#include <armnn/ArmNN.hpp>

#include <backendsCommon/test/TensorCopyUtils.hpp>
#include <backendsCommon/test/WorkloadTestUtils.hpp>

#include <test/TensorHelpers.hpp>

namespace
{

template<typename T>
LayerTestResult<T, 4> SpaceToBatchNdTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    armnn::TensorInfo& inputTensorInfo,
    armnn::TensorInfo& outputTensorInfo,
    std::vector<float>& inputData,
    std::vector<float>& outputExpectedData,
    armnn::SpaceToBatchNdQueueDescriptor descriptor,
    const float qScale = 1.0f,
    const int32_t qOffset = 0)
{
    const armnn::PermutationVector NCHWToNHWC = {0, 3, 1, 2};
    if (descriptor.m_Parameters.m_DataLayout == armnn::DataLayout::NHWC)
    {
        inputTensorInfo = armnnUtils::Permuted(inputTensorInfo, NCHWToNHWC);
        outputTensorInfo = armnnUtils::Permuted(outputTensorInfo, NCHWToNHWC);

        std::vector<float> inputTmp(inputData.size());
        armnnUtils::Permute(inputTensorInfo.GetShape(), NCHWToNHWC,
                            inputData.data(), inputTmp.data(), sizeof(float));
        inputData = inputTmp;

        std::vector<float> outputTmp(outputExpectedData.size());
        armnnUtils::Permute(outputTensorInfo.GetShape(), NCHWToNHWC,
                            outputExpectedData.data(), outputTmp.data(), sizeof(float));
        outputExpectedData = outputTmp;
    }

    if(armnn::IsQuantizedType<T>())
    {
        inputTensorInfo.SetQuantizationScale(qScale);
        inputTensorInfo.SetQuantizationOffset(qOffset);
        outputTensorInfo.SetQuantizationScale(qScale);
        outputTensorInfo.SetQuantizationOffset(qOffset);
    }

    boost::multi_array<T, 4> input = MakeTensor<T, 4>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, inputData));

    LayerTestResult<T, 4> ret(outputTensorInfo);
    ret.outputExpected = MakeTensor<T, 4>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, outputExpectedData));

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::WorkloadInfo info;
    AddInputToWorkload(descriptor, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateSpaceToBatchNd(descriptor, info);

    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    workload->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());

    return ret;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SpaceToBatchNdSimpleTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    armnn::DataLayout dataLayout = armnn::DataLayout::NCHW)
{
    armnn::TensorInfo inputTensorInfo;
    armnn::TensorInfo outputTensorInfo;

    unsigned int inputShape[] = {1, 1, 2, 2};
    unsigned int outputShape[] = {4, 1, 1, 1};

    armnn::SpaceToBatchNdQueueDescriptor desc;
    desc.m_Parameters.m_DataLayout = dataLayout;
    desc.m_Parameters.m_BlockShape = {2, 2};
    desc.m_Parameters.m_PadList = {{0, 0}, {0, 0}};

    inputTensorInfo = armnn::TensorInfo(4, inputShape, ArmnnType);
    outputTensorInfo = armnn::TensorInfo(4, outputShape, ArmnnType);

    std::vector<float> input = std::vector<float>(
    {
        1.0f, 2.0f, 3.0f, 4.0f
    });

    std::vector<float> outputExpected = std::vector<float>(
    {
        1.0f, 2.0f, 3.0f, 4.0f
    });

    return SpaceToBatchNdTestImpl<T>(
        workloadFactory, memoryManager, inputTensorInfo, outputTensorInfo, input, outputExpected, desc);
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SpaceToBatchNdMultiChannelsTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    armnn::DataLayout dataLayout = armnn::DataLayout::NCHW)
{
    armnn::TensorInfo inputTensorInfo;
    armnn::TensorInfo outputTensorInfo;

    unsigned int inputShape[] = {1, 3, 2, 2};
    unsigned int outputShape[] = {4, 3, 1, 1};

    armnn::SpaceToBatchNdQueueDescriptor desc;
    desc.m_Parameters.m_DataLayout = dataLayout;
    desc.m_Parameters.m_BlockShape = {2, 2};
    desc.m_Parameters.m_PadList = {{0, 0}, {0, 0}};

    inputTensorInfo = armnn::TensorInfo(4, inputShape, ArmnnType);
    outputTensorInfo = armnn::TensorInfo(4, outputShape, ArmnnType);

    std::vector<float> input = std::vector<float>(
    {
        1.0f, 4.0f, 7.0f, 10.0f,
        2.0f, 5.0, 8.0, 11.0f,
        3.0f, 6.0f, 9.0f, 12.0f
    });

    std::vector<float> outputExpected = std::vector<float>(
    {
        1.0f, 2.0f, 3.0f,
        4.0f, 5.0f, 6.0f,
        7.0f, 8.0f, 9.0f,
        10.0f, 11.0f, 12.0f
    });

    return SpaceToBatchNdTestImpl<T>(
        workloadFactory, memoryManager, inputTensorInfo, outputTensorInfo, input, outputExpected, desc);
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SpaceToBatchNdMultiBlockTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    armnn::DataLayout dataLayout = armnn::DataLayout::NCHW)
{
    armnn::TensorInfo inputTensorInfo;
    armnn::TensorInfo outputTensorInfo;

    unsigned int inputShape[] = {1, 1, 4, 4};
    unsigned int outputShape[] = {4, 1, 2, 2};

    armnn::SpaceToBatchNdQueueDescriptor desc;
    desc.m_Parameters.m_DataLayout = dataLayout;
    desc.m_Parameters.m_BlockShape = {2, 2};
    desc.m_Parameters.m_PadList = {{0, 0}, {0, 0}};

    inputTensorInfo = armnn::TensorInfo(4, inputShape, ArmnnType);
    outputTensorInfo = armnn::TensorInfo(4, outputShape, ArmnnType);

    std::vector<float> input = std::vector<float>(
    {
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f,
        13.0f, 14.0f, 15.0f, 16.0f
    });

    std::vector<float> outputExpected = std::vector<float>(
    {
        1.0f, 3.0f, 9.0f, 11.0f,
        2.0f, 4.0f, 10.0f, 12.0f,
        5.0f, 7.0f, 13.0f, 15.0f,
        6.0f, 8.0f, 14.0f, 16.0f
    });

    return SpaceToBatchNdTestImpl<T>(
        workloadFactory, memoryManager, inputTensorInfo, outputTensorInfo, input, outputExpected, desc);
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SpaceToBatchNdPaddingTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    armnn::DataLayout dataLayout = armnn::DataLayout::NCHW)
{
    armnn::TensorInfo inputTensorInfo;
    armnn::TensorInfo outputTensorInfo;

    unsigned int inputShape[] = {2, 1, 2, 4};
    unsigned int outputShape[] = {8, 1, 1, 3};

    armnn::SpaceToBatchNdQueueDescriptor desc;
    desc.m_Parameters.m_DataLayout = dataLayout;
    desc.m_Parameters.m_BlockShape = {2, 2};
    desc.m_Parameters.m_PadList = {{0, 0}, {2, 0}};

    inputTensorInfo = armnn::TensorInfo(4, inputShape, ArmnnType);
    outputTensorInfo = armnn::TensorInfo(4, outputShape, ArmnnType);

    std::vector<float> input = std::vector<float>(
    {
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f,
        13.0f, 14.0f, 15.0f, 16.0f
    });

    std::vector<float> outputExpected = std::vector<float>(
    {
        0.0f, 1.0f, 3.0f,
        0.0f, 9.0f, 11.0f,
        0.0f, 2.0f, 4.0f,
        0.0f, 10.0f, 12.0f,
        0.0f, 5.0f, 7.0f,
        0.0f, 13.0f, 15.0f,
        0.0f, 6.0f, 8.0f,
        0.0f, 14.0f, 16.0f
    });

    return SpaceToBatchNdTestImpl<T>(
        workloadFactory, memoryManager, inputTensorInfo, outputTensorInfo, input, outputExpected, desc);
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SpaceToBatchNdSimpleNhwcTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdSimpleTest<ArmnnType>(workloadFactory, memoryManager, armnn::DataLayout::NHWC);
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SpaceToBatchNdMultiChannelsNhwcTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiChannelsTest<ArmnnType>(workloadFactory, memoryManager, armnn::DataLayout::NHWC);
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SpaceToBatchNdMultiBlockNhwcTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiBlockTest<ArmnnType>(workloadFactory, memoryManager, armnn::DataLayout::NHWC);
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SpaceToBatchNdPaddingNhwcTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdPaddingTest<ArmnnType>(workloadFactory, memoryManager, armnn::DataLayout::NHWC);
}

} // anonymous namespace

LayerTestResult<float, 4> SpaceToBatchNdSimpleFloat32Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdSimpleTest<armnn::DataType::Float32>(workloadFactory, memoryManager);
}

LayerTestResult<float, 4> SpaceToBatchNdMultiChannelsFloat32Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiChannelsTest<armnn::DataType::Float32>(workloadFactory, memoryManager);
}

LayerTestResult<float, 4> SpaceToBatchNdMultiBlockFloat32Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiBlockTest<armnn::DataType::Float32>(workloadFactory, memoryManager);
}

LayerTestResult<float, 4> SpaceToBatchNdPaddingFloat32Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdPaddingTest<armnn::DataType::Float32>(workloadFactory, memoryManager);
}

LayerTestResult<armnn::Half, 4> SpaceToBatchNdSimpleFloat16Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdSimpleTest<armnn::DataType::Float16>(workloadFactory, memoryManager);
}

LayerTestResult<armnn::Half, 4> SpaceToBatchNdMultiChannelsFloat16Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiChannelsTest<armnn::DataType::Float16>(workloadFactory, memoryManager);
}

LayerTestResult<armnn::Half, 4> SpaceToBatchNdMultiBlockFloat16Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiBlockTest<armnn::DataType::Float16>(workloadFactory, memoryManager);
}

LayerTestResult<armnn::Half, 4> SpaceToBatchNdPaddingFloat16Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdPaddingTest<armnn::DataType::Float16>(workloadFactory, memoryManager);
}

LayerTestResult<uint8_t, 4> SpaceToBatchNdSimpleUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdSimpleTest<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager);
}

LayerTestResult<uint8_t, 4> SpaceToBatchNdMultiChannelsUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiChannelsTest<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager);
}

LayerTestResult<uint8_t, 4> SpaceToBatchNdMultiBlockUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiBlockTest<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager);
}

LayerTestResult<uint8_t, 4> SpaceToBatchNdPaddingUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdPaddingTest<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager);
}

LayerTestResult<float, 4> SpaceToBatchNdSimpleNhwcFloat32Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdSimpleNhwcTest<armnn::DataType::Float32>(workloadFactory, memoryManager);
}

LayerTestResult<float, 4> SpaceToBatchNdMultiChannelsNhwcFloat32Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiChannelsNhwcTest<armnn::DataType::Float32>(workloadFactory, memoryManager);
}

LayerTestResult<float, 4> SpaceToBatchNdMultiBlockNhwcFloat32Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiBlockNhwcTest<armnn::DataType::Float32>(workloadFactory, memoryManager);
}

LayerTestResult<float, 4> SpaceToBatchNdPaddingNhwcFloat32Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdPaddingNhwcTest<armnn::DataType::Float32>(workloadFactory, memoryManager);
}

LayerTestResult<armnn::Half, 4> SpaceToBatchNdSimpleNhwcFloat16Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdSimpleNhwcTest<armnn::DataType::Float16>(workloadFactory, memoryManager);
}

LayerTestResult<armnn::Half, 4> SpaceToBatchNdMultiChannelsNhwcFloat16Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiChannelsNhwcTest<armnn::DataType::Float16>(workloadFactory, memoryManager);
}

LayerTestResult<armnn::Half, 4> SpaceToBatchNdMultiBlockNhwcFloat16Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiBlockNhwcTest<armnn::DataType::Float16>(workloadFactory, memoryManager);
}

LayerTestResult<armnn::Half, 4> SpaceToBatchNdPaddingNhwcFloat16Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdPaddingNhwcTest<armnn::DataType::Float16>(workloadFactory, memoryManager);
}

LayerTestResult<uint8_t, 4> SpaceToBatchNdSimpleNhwcUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdSimpleNhwcTest<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager);
}

LayerTestResult<uint8_t, 4> SpaceToBatchNdMultiChannelsNhwcUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiChannelsNhwcTest<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager);
}

LayerTestResult<uint8_t, 4> SpaceToBatchNdMultiBlockNhwcUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiBlockNhwcTest<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager);
}

LayerTestResult<uint8_t, 4> SpaceToBatchNdPaddingNhwcUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdPaddingNhwcTest<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager);
}

LayerTestResult<int16_t, 4> SpaceToBatchNdSimpleUint16Test(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdSimpleTest<armnn::DataType::QuantisedSymm16>(workloadFactory, memoryManager);
}

LayerTestResult<int16_t, 4> SpaceToBatchNdMultiChannelsUint16Test(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiChannelsTest<armnn::DataType::QuantisedSymm16>(workloadFactory, memoryManager);
}

LayerTestResult<int16_t, 4> SpaceToBatchNdMultiBlockUint16Test(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiBlockTest<armnn::DataType::QuantisedSymm16>(workloadFactory, memoryManager);
}

LayerTestResult<int16_t, 4> SpaceToBatchNdPaddingUint16Test(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdPaddingTest<armnn::DataType::QuantisedSymm16>(workloadFactory, memoryManager);
}

LayerTestResult<int16_t, 4> SpaceToBatchNdSimpleNhwcUint16Test(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdSimpleNhwcTest<armnn::DataType::QuantisedSymm16>(workloadFactory, memoryManager);
}

LayerTestResult<int16_t, 4> SpaceToBatchNdMultiChannelsNhwcUint16Test(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiChannelsNhwcTest<armnn::DataType::QuantisedSymm16>(workloadFactory, memoryManager);
}

LayerTestResult<int16_t, 4> SpaceToBatchNdMultiBlockNhwcUint16Test(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdMultiBlockNhwcTest<armnn::DataType::QuantisedSymm16>(workloadFactory, memoryManager);
}

LayerTestResult<int16_t, 4> SpaceToBatchNdPaddingNhwcUint16Test(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SpaceToBatchNdPaddingNhwcTest<armnn::DataType::QuantisedSymm16>(workloadFactory, memoryManager);
}
