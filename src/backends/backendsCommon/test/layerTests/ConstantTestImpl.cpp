//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "ConstantTestImpl.hpp"

#include <Permute.hpp>
#include <ResolveType.hpp>

#include <armnn/ArmNN.hpp>

#include <backendsCommon/CpuTensorHandle.hpp>

#include <backendsCommon/test/TensorCopyUtils.hpp>
#include <backendsCommon/test/WorkloadTestUtils.hpp>

#include <test/TensorHelpers.hpp>

namespace
{

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> ConstantTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    constexpr unsigned int inputWidth = 3;
    constexpr unsigned int inputHeight = 4;
    constexpr unsigned int inputChannels = 3;
    constexpr unsigned int inputBatchSize = 2;

    constexpr unsigned int outputWidth = inputWidth;
    constexpr unsigned int outputHeight = inputHeight;
    constexpr unsigned int outputChannels = inputChannels;
    constexpr unsigned int outputBatchSize = inputBatchSize;

    armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
                                        ArmnnType, qScale, qOffset);

    armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
                                         ArmnnType, qScale, qOffset);

    // Set quantization parameters if the requested type is a quantized type.
    if(armnn::IsQuantizedType<T>())
    {
        inputTensorInfo.SetQuantizationScale(qScale);
        inputTensorInfo.SetQuantizationOffset(qOffset);
        outputTensorInfo.SetQuantizationScale(qScale);
        outputTensorInfo.SetQuantizationOffset(qOffset);
    }

    auto input = MakeTensor<T, 4>(inputTensorInfo, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        235.0f,  46.0f, 178.0f,
        100.0f, 123.0f,  19.0f,
        172.0f,  74.0f, 250.0f,
          6.0f, 195.0f,  80.0f,

        // Batch 0, Channel 1
        113.0f,  95.0f, 202.0f,
         77.0f, 114.0f,  71.0f,
        122.0f, 246.0f, 166.0f,
         82.0f,  28.0f,  37.0f,

        // Batch 0, Channel 2
         56.0f, 170.0f, 162.0f,
        194.0f,  89.0f, 254.0f,
         12.0f, 209.0f, 200.0f,
          1.0f,  64.0f,  54.0f,

        // Batch 1, Channel 0
         67.0f,  90.0f,  49.0f,
          7.0f, 163.0f,  18.0f,
         25.0f, 117.0f, 103.0f,
        247.0f,  59.0f, 189.0f,

        // Batch 1, Channel 1
        239.0f, 104.0f, 199.0f,
         17.0f, 124.0f, 153.0f,
        222.0f, 217.0f, 75.0f,
         32.0f, 126.0f, 21.0f,

        // Batch 1, Channel 2
         97.0f, 145.0f, 215.0f,
        115.0f, 116.0f, 238.0f,
        226.0f,  16.0f, 132.0f,
         92.0f, 125.0f,  88.0f,
    })));

    LayerTestResult<T, 4> result(outputTensorInfo);
    result.outputExpected = input;

    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ScopedCpuTensorHandle constantTensor(inputTensorInfo);
    AllocateAndCopyDataToITensorHandle(&constantTensor, &input[0][0][0][0]);

    armnn::ConstantQueueDescriptor descriptor;
    descriptor.m_LayerOutput = &constantTensor;

    armnn::WorkloadInfo info;
    AddOutputToWorkload(descriptor, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateConstant(descriptor, info);

    outputHandle->Allocate();

    workload->PostAllocationConfigure();
    workload->Execute();

    CopyDataFromITensorHandle(&result.output[0][0][0][0], outputHandle.get());
    return result;
}

} // anonymous namespace

LayerTestResult<float, 4> ConstantTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return ConstantTestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<int16_t, 4> ConstantInt16SimpleQuantizationScaleNoOffsetTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return ConstantTestImpl<armnn::DataType::QuantisedSymm16>(workloadFactory, memoryManager, 1.0f, 0);
}

LayerTestResult<uint8_t, 4> ConstantUint8SimpleQuantizationScaleNoOffsetTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return ConstantTestImpl<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager, 1.0f, 0);
}

LayerTestResult<uint8_t, 4> ConstantUint8CustomQuantizationScaleAndOffsetTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return ConstantTestImpl<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager, 2e-6f, 1);
}

LayerTestResult<int16_t, 4> ConstantInt16CustomQuantizationScaleAndOffsetTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return ConstantTestImpl<armnn::DataType::QuantisedSymm16>(workloadFactory, memoryManager, 2e-6f, 1);
}
