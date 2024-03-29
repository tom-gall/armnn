//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "Conv2dTestImpl.hpp"

#include <DataLayoutIndexed.hpp>
#include <Permute.hpp>
#include <TensorUtils.hpp>

#include <armnn/ArmNN.hpp>

#include <backendsCommon/CpuTensorHandle.hpp>

#include <backendsCommon/test/QuantizeHelper.hpp>
#include <backendsCommon/test/TensorCopyUtils.hpp>
#include <backendsCommon/test/WorkloadTestUtils.hpp>

#include <test/TensorHelpers.hpp>

#include <boost/numeric/conversion/cast.hpp>

#include <string>

//
// Static data
//

// 2-channel bias used by a number of Conv2d tests.
static std::vector<float> Bias2({0, 2});

static std::vector<float> Bias4({1, 2, 3, 4});

static std::vector<float> Bias8({1, 2, 3, 4, 1, 2, 3, 4});

// 3-channel 16x8 image used as common input data for a number of Conv2d tests.
static std::vector<float> ConvInput3x8x16({
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
});

//
// Helper templates
//

// Helper template that returns either Bias2 or an empty vector depending on whether bias is enabled.
template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
boost::multi_array<T, 1> GetBias2(bool biasEnabled, float qScale)
{
    if(biasEnabled)
    {
        armnn::TensorInfo biasDesc({static_cast<unsigned int>(Bias2.size())}, ArmnnType);
        boost::multi_array<T, 1> bias = MakeTensor<T, 1>(biasDesc, QuantizedVector<T>(qScale, 0.0f, Bias2));
        return bias;
    }
    else
    {
        return boost::multi_array<T, 1>();
    }
}

// Helper template that returns either Bias4 or an empty vector depending on whether bias is enabled.
template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
boost::multi_array<T, 1> GetBias4(bool biasEnabled, float qScale)
{
    if(biasEnabled)
    {
        armnn::TensorInfo biasDesc({static_cast<unsigned int>(Bias4.size())}, ArmnnType);
        boost::multi_array<T, 1> bias = MakeTensor<T, 1>(biasDesc, QuantizedVector<T>(qScale, 0.0f, Bias4));
        return bias;
    }
    else
    {
        return boost::multi_array<T, 1>();
    }
}

// Helper template that returns either Bias8 or an empty vector depending on whether bias is enabled.
template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
boost::multi_array<T, 1> GetBias8(bool biasEnabled, float qScale)
{
    if(biasEnabled)
    {
        armnn::TensorInfo biasDesc({static_cast<unsigned int>(Bias4.size())}, ArmnnType);
        boost::multi_array<T, 1> bias = MakeTensor<T, 1>(biasDesc, QuantizedVector<T>(qScale, 0.0f, Bias8));
        return bias;
    }
    else
    {
        return boost::multi_array<T, 1>();
    }
}

// Helper template that returns either Bias4 or an empty vector depending on whether bias is enabled.
template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
boost::multi_array<T, 1> GetBias(bool biasEnabled, float qScale, armnn::TensorInfo outputInfo, armnn::DataLayout layout)
{
    const armnnUtils::DataLayoutIndexed dataLayoutIndexed(layout);
    const unsigned int channelsIndex = dataLayoutIndexed.GetChannelsIndex();
    const unsigned int outputChannels = outputInfo.GetShape()[channelsIndex];

    switch (outputChannels)
    {
        case 2:
        default:
        {
            return GetBias2<ArmnnType>(biasEnabled, qScale);
        }
        case 4:
        {
            return GetBias4<ArmnnType>(biasEnabled, qScale);
        }
        case 8:
        {
            return GetBias8<ArmnnType>(biasEnabled, qScale);
        }
    }
}

//
// Implementation templates
//

// Mapping from input type to bias type for fully connected layers.
// float => float, uint8_t => int32_t
template<typename T>
struct FullyConnectedBiasTypeForInputType;

template<>
struct FullyConnectedBiasTypeForInputType<float>
{
    using Type = float;
};

template<>
struct FullyConnectedBiasTypeForInputType<uint8_t>
{
    using Type = int32_t;
};

// Modifies a std::vector in-place using a specified bias.
template<typename T, typename B>
void ApplyBias(std::vector<T>& v, float vScale, int32_t vOffset,
    const std::vector<B>& bias, float bScale, int32_t bOffset, uint32_t w, uint32_t h)
{
    BOOST_ASSERT_MSG((armnn::IsQuantizedType<T>() && vScale != 0.0f) || (!armnn::IsQuantizedType<T>()),
                     "Invalid type and parameter combination.");
    BOOST_ASSERT_MSG((armnn::IsQuantizedType<B>() && bScale != 0.0f) || (!armnn::IsQuantizedType<B>()),
                     "Invalid type and parameter combination.");

    // Note we need to dequantize and re-quantize the image value and the bias.
    for (uint32_t i = 0; i < bias.size(); ++i)
    {
        float dBias = SelectiveDequantize(bias[i], bScale, bOffset);
        for (uint32_t y = 0; y < h; ++y)
        {
            for (uint32_t x = 0; x < w; ++x)
            {
                uint32_t offset = (i * h + y) * w + x;
                BOOST_ASSERT(offset < v.size());
                T& outRef = v[offset];
                float dOutput = SelectiveDequantize(outRef, vScale, vOffset);
                outRef = SelectiveQuantize<T>(dOutput + dBias, vScale, vOffset);
            }
        }
    }
}

//
// Convolution2d implementations
//

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType,
         typename T = armnn::ResolveType<ArmnnType>, typename B = armnn::ResolveType<ArmnnBType>>
LayerTestResult<T, 4> SimpleConvolution2dTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    const boost::multi_array<T, 4>& originalInput,
    const boost::multi_array<T, 4>& originalKernel,
    const boost::multi_array<B, 1>& bias,
    const boost::multi_array<T, 4>& originalOutputExpected,
    float qScale,
    int32_t qOffset,
    const armnn::DataLayout layout = armnn::DataLayout::NCHW,
    uint32_t padLeft = 0,
    uint32_t padTop = 0,
    uint32_t padRight = 0,
    uint32_t padBottom = 0,
    uint32_t strideX = 1,
    uint32_t strideY = 1,
    uint32_t dilationX = 1,
    uint32_t dilationY = 1)
{
    unsigned int inputHeight   = boost::numeric_cast<unsigned int>(originalInput.shape()[2]);
    unsigned int inputWidth    = boost::numeric_cast<unsigned int>(originalInput.shape()[3]);
    unsigned int inputChannels = boost::numeric_cast<unsigned int>(originalInput.shape()[1]);
    unsigned int inputNum      = boost::numeric_cast<unsigned int>(originalInput.shape()[0]);

    unsigned int outputHeight   = boost::numeric_cast<unsigned int>(originalOutputExpected.shape()[2]);
    unsigned int outputWidth    = boost::numeric_cast<unsigned int>(originalOutputExpected.shape()[3]);
    unsigned int outputChannels = boost::numeric_cast<unsigned int>(originalOutputExpected.shape()[1]);
    unsigned int outputNum      = boost::numeric_cast<unsigned int>(originalOutputExpected.shape()[0]);

    unsigned int kernelHeight = boost::numeric_cast<unsigned int>(originalKernel.shape()[2]);
    unsigned int kernelWidth = boost::numeric_cast<unsigned int>(originalKernel.shape()[3]);
    unsigned int kernelChannels = boost::numeric_cast<unsigned int>(originalKernel.shape()[1]);
    unsigned int kernelDepthMul = boost::numeric_cast<unsigned int>(originalKernel.shape()[0]);

    bool biasEnabled = bias.size() > 0;

    // This function currently assumes 1 batch of input/output (and duplicates this into 2 batches).
    BOOST_ASSERT(inputNum == 1);
    BOOST_ASSERT(outputNum == 1);

    // If a bias is used, its size must equal the number of output channels.
    BOOST_ASSERT(!biasEnabled || bias.size() == outputChannels);


    // Note these tensors will use two (identical) batches.
    armnn::TensorInfo inputTensorInfo =
            armnnUtils::GetTensorInfo(2*inputNum, inputChannels, inputHeight, inputWidth, layout, ArmnnType);
    armnn::TensorInfo outputTensorInfo =
            armnnUtils::GetTensorInfo(2*outputNum, outputChannels, outputHeight, outputWidth, layout, ArmnnType);
    armnn::TensorInfo kernelDesc =
            armnnUtils::GetTensorInfo(kernelDepthMul, kernelChannels, kernelHeight, kernelWidth, layout, ArmnnType);
    armnn::TensorInfo biasDesc({static_cast<unsigned int>(bias.size())}, ArmnnBType);

    // Set quantization parameters if the requested type is a quantized type.
    if(armnn::IsQuantizedType<T>())
    {
        inputTensorInfo.SetQuantizationScale(qScale);
        inputTensorInfo.SetQuantizationOffset(qOffset);
        outputTensorInfo.SetQuantizationScale(qScale);
        outputTensorInfo.SetQuantizationOffset(qOffset);
        kernelDesc.SetQuantizationScale(qScale);
        kernelDesc.SetQuantizationOffset(qOffset);
        biasDesc.SetQuantizationScale(qScale*qScale);
        biasDesc.SetQuantizationOffset(0);
    }

    LayerTestResult<T, 4> ret(outputTensorInfo);

    // Construct input data - two batches of the same input image.
    std::vector<T> inputImage;
    inputImage.assign(originalInput.data(), originalInput.data() + 1*inputChannels*inputHeight*inputWidth);
    std::vector<T> inputData;
    inputData.insert(inputData.end(), inputImage.begin(), inputImage.end());
    inputData.insert(inputData.end(), inputImage.begin(), inputImage.end());

    // at this point if we require it permute the input data
    const armnn::PermutationVector NCHWToNHWC = { 0, 3, 1, 2 };
    if (layout == armnn::DataLayout::NHWC)
    {
        std::vector<T> tmp(inputData.size());
        armnnUtils::Permute(inputTensorInfo.GetShape(), NCHWToNHWC, inputData.data(), tmp.data(), sizeof(T));
        inputData = tmp;
    }

    auto batchedInput = MakeTensor<T, 4>(inputTensorInfo, inputData);

    std::vector<T> outputImage;
    outputImage.assign(originalOutputExpected.data(),
            originalOutputExpected.data() + outputChannels*outputHeight*outputWidth);

    // Apply bias to output image if it is enabled.
    if(biasEnabled)
    {
        std::vector<T> biasV;
        biasV.assign(bias.data(), bias.data() + outputChannels);
        ApplyBias(outputImage, outputTensorInfo.GetQuantizationScale(), outputTensorInfo.GetQuantizationOffset(),
            biasV, biasDesc.GetQuantizationScale(), biasDesc.GetQuantizationOffset(),
            outputWidth, outputHeight);
    }

    // Construct expected output data - two identical images.
    std::vector<T> outputData;
    outputData.insert(outputData.end(), outputImage.begin(), outputImage.end());
    outputData.insert(outputData.end(), outputImage.begin(), outputImage.end());

    // at this point if we require it permute the expected output
    if (layout == armnn::DataLayout::NHWC)
    {
        std::vector<T> tmp(outputData.size());
        armnnUtils::Permute(outputTensorInfo.GetShape(), NCHWToNHWC, outputData.data(), tmp.data(), sizeof(T));
        outputData = tmp;
    }
    ret.outputExpected = MakeTensor<T, 4>(outputTensorInfo, outputData);

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::Convolution2dQueueDescriptor data;
    armnn::WorkloadInfo info;
    armnn::ScopedCpuTensorHandle weightsTensor(kernelDesc);
    armnn::ScopedCpuTensorHandle biasTensor(biasDesc);
    // Permute the kernel if necessary
    boost::multi_array<T, 4> kernel = boost::multi_array<T, 4>(originalKernel);
    if (layout == armnn::DataLayout::NHWC)
    {
        armnnUtils::Permute(kernelDesc.GetShape(), NCHWToNHWC, originalKernel.data(), kernel.data(), sizeof(T));
    }
    AllocateAndCopyDataToITensorHandle(&weightsTensor, &kernel[0][0][0][0]);

    if(biasEnabled)
    {
        AllocateAndCopyDataToITensorHandle(&biasTensor, &bias[0]);
    }

    AddInputToWorkload(data, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    data.m_Weight = &weightsTensor;
    data.m_Bias = &biasTensor; // Still set this whether or not bias is enabled - can be a source of bugs.
    data.m_Parameters.m_StrideX = strideX;
    data.m_Parameters.m_StrideY = strideY;
    data.m_Parameters.m_PadLeft = padLeft;
    data.m_Parameters.m_PadRight = padRight;
    data.m_Parameters.m_PadTop = padTop;
    data.m_Parameters.m_PadBottom = padBottom;
    data.m_Parameters.m_BiasEnabled = biasEnabled;
    data.m_Parameters.m_DataLayout = layout;
    data.m_Parameters.m_DilationX = dilationX;
    data.m_Parameters.m_DilationY = dilationY;

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateConvolution2d(data, info);
    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), &batchedInput[0][0][0][0]);

    ExecuteWorkload(*workload, memoryManager);

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());

    return ret;
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType,
         typename T = armnn::ResolveType<ArmnnType>, typename B = armnn::ResolveType<ArmnnBType>>
LayerTestResult<T, 4> SimpleConvolution2dNhwcTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    const boost::multi_array<T, 4>& input,
    const boost::multi_array<T, 4>& kernel,
    const boost::multi_array<B, 1>& bias,
    const boost::multi_array<T, 4>& outputExpected,
    const armnn::DataLayout dataLayout,
    float qScale,
    int32_t qOffset,
    uint32_t padLeft = 1,
    uint32_t padTop = 1,
    uint32_t padRight = 1,
    uint32_t padBottom = 1,
    uint32_t strideX  = 1,
    uint32_t strideY  = 1)
{
    unsigned int inputNum       = boost::numeric_cast<unsigned int>(input.shape()[0]);
    unsigned int inputChannels  = boost::numeric_cast<unsigned int>(input.shape()[3]);
    unsigned int inputHeight    = boost::numeric_cast<unsigned int>(input.shape()[1]);
    unsigned int inputWidth     = boost::numeric_cast<unsigned int>(input.shape()[2]);

    unsigned int kernelChanMul  = boost::numeric_cast<unsigned int>(kernel.shape()[0]);
    unsigned int kernelChannels = boost::numeric_cast<unsigned int>(kernel.shape()[3]);
    unsigned int kernelHeight   = boost::numeric_cast<unsigned int>(kernel.shape()[1]);
    unsigned int kernelWidth    = boost::numeric_cast<unsigned int>(kernel.shape()[2]);

    unsigned int outputNum      = boost::numeric_cast<unsigned int>(outputExpected.shape()[0]);
    unsigned int outputChannels = boost::numeric_cast<unsigned int>(outputExpected.shape()[3]);
    unsigned int outputHeight   = boost::numeric_cast<unsigned int>(outputExpected.shape()[1]);
    unsigned int outputWidth    = boost::numeric_cast<unsigned int>(outputExpected.shape()[2]);

    bool biasEnabled = bias.size() > 0;

    // Creates the tensors.
    armnn::TensorInfo inputTensorInfo({inputNum, inputHeight, inputWidth, inputChannels}, ArmnnType);
    armnn::TensorInfo outputTensorInfo({outputNum, outputHeight, outputWidth, outputChannels},
                                       ArmnnType);
    armnn::TensorInfo kernelDesc({kernelChanMul, kernelHeight, kernelWidth, kernelChannels}, ArmnnType);
    armnn::TensorInfo biasDesc({static_cast<unsigned int>(bias.size())}, ArmnnBType);

    // Construct the input data.
    std::vector<T> inputData;
    inputData.assign(input.data(), input.data() + inputHeight*inputWidth*inputChannels);
    auto batchedInput = MakeTensor<T, 4>(inputTensorInfo, inputData);

    // Construct the output data, with bias applied, as appropriate.
    std::vector<T> outputData;
    outputData.assign(outputExpected.data(), outputExpected.data() + outputHeight*outputWidth*outputChannels);

    LayerTestResult<T, 4> ret(outputTensorInfo);
    ret.outputExpected = MakeTensor<T, 4>(outputTensorInfo, outputData);

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ScopedCpuTensorHandle weightsTensor(kernelDesc);
    AllocateAndCopyDataToITensorHandle(&weightsTensor, &kernel[0][0][0][0]);

    armnn::ScopedCpuTensorHandle biasTensor(biasDesc);

    armnn::Convolution2dQueueDescriptor data;

    data.m_Weight = &weightsTensor;
    data.m_Bias = &biasTensor; // Still set this whether or not bias is enabled - can be a source of bugs.
    data.m_Parameters.m_StrideX = strideX;
    data.m_Parameters.m_StrideY = strideY;
    data.m_Parameters.m_PadLeft = padLeft;
    data.m_Parameters.m_PadRight = padRight;
    data.m_Parameters.m_PadTop = padTop;
    data.m_Parameters.m_PadBottom = padBottom;
    data.m_Parameters.m_BiasEnabled = biasEnabled;
    data.m_Parameters.m_DataLayout = dataLayout;

    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateConvolution2d(data, info);
    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), &batchedInput[0][0][0][0]);

    ExecuteWorkload(*workload, memoryManager);

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());

    return ret;
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T,4> Convolution1dTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset,
    bool biasEnabled)
{
    using B = armnn::ResolveType<ArmnnBType>;
    // Until we have a specialist 1D convolution layer, we can fake one using
    // 2D convolution with the final dimension set to 1.
    // I don't anticipate this being particularly slow, given that convolution is implemented
    // as a matrix multiplication, at which point dimension doesn't matter.

    unsigned int batchSize      = 1;
    unsigned int inputChannels  = 2;
    unsigned int outputChannels = 3;
    unsigned int inputSize      = 5; // The 1D size (could view as 'width' or 'height').
    unsigned int kernelSize     = 3;
    unsigned int padSize        = 2;
    unsigned int stride         = 1;
    unsigned int outputSize     = 7; // (inputSize + 2 * padSize - kernelSize + 1) / stride.

    armnn::TensorInfo inputInfo({batchSize, inputChannels, inputSize, 1}, ArmnnType);
    armnn::TensorInfo outputInfo({batchSize, outputChannels, outputSize, 1}, ArmnnType);
    armnn::TensorInfo kernelInfo({outputChannels, inputChannels, kernelSize, 1}, ArmnnType);
    armnn::TensorInfo biasInfo({outputChannels}, ArmnnBType);

    // Set quantization parameters if the requested type is a quantized type.
    if(armnn::IsQuantizedType<T>())
    {
        inputInfo.SetQuantizationScale(qScale);
        inputInfo.SetQuantizationOffset(qOffset);
        outputInfo.SetQuantizationScale(qScale);
        outputInfo.SetQuantizationOffset(qOffset);
        kernelInfo.SetQuantizationScale(qScale);
        kernelInfo.SetQuantizationOffset(qOffset);
        biasInfo.SetQuantizationScale(inputInfo.GetQuantizationScale()*kernelInfo.GetQuantizationScale());
        biasInfo.SetQuantizationOffset(0);
    }

    std::vector<T> inputData(
        QuantizedVector<T>(inputInfo.GetQuantizationScale(), inputInfo.GetQuantizationOffset(), {
            5.0f, -2.0f, 2.5f, 0.0f, 1.0f,
            -3.0f, 3.2f, 5.0f, 2.0f, 3.0f,
        }));

    std::vector<T> kernelData(
        QuantizedVector<T>(kernelInfo.GetQuantizationScale(), kernelInfo.GetQuantizationOffset(), {
            1.0f, 0.0f, 0.0f,
            0.0f, 2.0f, -1.5f,

            0.0f, 0.0f, 0.0f,
            0.2f, 0.2f, 0.2f,

            0.5f, 0.0f, 0.5f,
            0.0f, -1.0f, 0.0f
        }));

    std::vector<B> biasData(
        QuantizedVector<B>(biasInfo.GetQuantizationScale(), biasInfo.GetQuantizationOffset(), {
            1.0f, 0.0f, 0.0f
        }));

    std::vector<T> outputData(
        QuantizedVector<T>(outputInfo.GetQuantizationScale(), outputInfo.GetQuantizationOffset(), {
            4.5f, -10.8f, 5.0f + 6.4f - 7.5f, -2.0f + 10.0f -3.0f, 2.5f + 4.0f - 4.5f, 6.0f, 1.0f,
            -0.6f, -0.6f + 0.64f, -0.6f + 0.64f + 1.0f, 0.64f + 1.0f + 0.4f, 1.0f + 0.4f + 0.6f, 0.4f + 0.6f, 0.6f,
            2.5f, -1.0f + 3.0f, 1.25f - 3.2f + 2.5f, -1.0f - 5.0f, 1.25f + 0.5f - 2.0f, -3.0f, 0.5f
        }));

    // Optionally apply bias to output image.
    if(biasEnabled)
    {
        ApplyBias(outputData, outputInfo.GetQuantizationScale(), outputInfo.GetQuantizationOffset(),
            biasData, biasInfo.GetQuantizationScale(), biasInfo.GetQuantizationOffset(),
            1, outputSize);
    }

    std::unique_ptr<armnn::ITensorHandle> inputHandle  = workloadFactory.CreateTensorHandle(inputInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputInfo);

    armnn::Convolution2dQueueDescriptor data;
    armnn::WorkloadInfo info;
    armnn::ScopedCpuTensorHandle         weightsTensor(kernelInfo);
    armnn::ScopedCpuTensorHandle         biasTensor(biasInfo);

    AllocateAndCopyDataToITensorHandle(&weightsTensor, kernelData.data());
    AllocateAndCopyDataToITensorHandle(&biasTensor, biasData.data());

    AddInputToWorkload(data, info, inputInfo, inputHandle.get());
    AddOutputToWorkload(data, info, outputInfo, outputHandle.get());

    data.m_Weight         = &weightsTensor;
    data.m_Bias           = &biasTensor;
    data.m_Parameters.m_StrideX        = 1;
    data.m_Parameters.m_StrideY        = stride;
    data.m_Parameters.m_PadLeft        = 0;
    data.m_Parameters.m_PadRight       = 0;
    data.m_Parameters.m_PadTop         = padSize;
    data.m_Parameters.m_PadBottom      = padSize;
    data.m_Parameters.m_BiasEnabled    = biasEnabled;

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateConvolution2d(data, info);
    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), inputData.data());

    ExecuteWorkload(*workload, memoryManager);

    // Output
    LayerTestResult<T,4> ret(outputInfo);
    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());
    ret.outputExpected = MakeTensor<T, 4>(outputInfo, outputData);
    return ret;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SimpleConvolution2d3x3NhwcTestCommon(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset,
    bool biasEnabled,
    armnn::DataLayout dataLayout)
{
    // Use common single-batch 5x5 image.

    armnn::TensorInfo inputDesc({1, 3, 4, 1}, ArmnnType);
    boost::multi_array<T, 4> input = MakeTensor<T, 4>(inputDesc,
                                                      {
                                                       1, 5, 2, 3,
                                                       8, 7, 3, 6,
                                                       3, 3, 9, 1
                                                       });


    // Use a 2-element batch of 3-channel 3x3 kernels.
    armnn::TensorInfo kernelDesc({1, 3, 3, 1}, ArmnnType);
    boost::multi_array<T, 4> kernel = MakeTensor<T, 4>(kernelDesc, {
                                                                    4, 5, 6,
                                                                    0, 0, 0,
                                                                    3, 2, 1
                                                                    });

    // Expected output is 1 batch of a 5x5 image.
    armnn::TensorInfo outputDesc({1, 3, 4, 1}, ArmnnType);

    const std::vector<float> outputData =
            {
                    23, 41, 33, 21,
                    44, 65, 76, 52,
                    82, 85, 79, 42
            };

    boost::multi_array<T, 4> expectedOutput = MakeTensor<T, 4>(outputDesc, outputData);

    return SimpleConvolution2dNhwcTestImpl<ArmnnType, ArmnnType>(
        workloadFactory,
        memoryManager,
        input,
        kernel,
        boost::multi_array<T, 1>(),
        expectedOutput,
        dataLayout,
        qScale,
        qOffset);
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SimpleConvolution2d3x3Stride2x2TestCommon(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
        float qScale,
        int32_t qOffset,
        bool biasEnabled,
        const armnn::DataLayout& dataLayout)
{
    // Input is a single-batch, 1 channel, 5x5 image.
    armnn::TensorInfo inputDesc({1, 5, 5, 1}, ArmnnType);
    boost::multi_array<T, 4> input = MakeTensor<T, 4>(inputDesc,
            {
                1, 5, 2, 3, 5,
                8, 7, 3, 6, 3,
                3, 3, 9, 1, 9,
                4, 1, 8, 1, 3,
                6, 8, 1, 9, 2
            });

    // Use a 3x3 kernel.
    armnn::TensorInfo kernelDesc({1, 3, 3, 1}, ArmnnType);
    boost::multi_array<T, 4> kernel = MakeTensor<T, 4>(kernelDesc,
            {
                4, 5, 6,
                0, 0, 0,
                3, 2, 1
            });

    // Expected output is a single-batch, 1 channel, 3x3 image.
    armnn::TensorInfo outputDesc({1, 3, 3, 1}, ArmnnType);

    const std::vector<T> outputData =
            {
                23, 33, 24,
                91, 99, 48,
                26, 50, 19
            };

    boost::multi_array<T, 4> expectedOutput = MakeTensor<T, 4>(outputDesc, outputData);

    uint32_t padLeft = 1;
    uint32_t padTop = 1;
    uint32_t padRight = 1;
    uint32_t padBottom = 1;
    uint32_t strideX  = 2;
    uint32_t strideY  = 2;

    return SimpleConvolution2dNhwcTestImpl<ArmnnType, ArmnnType>(
        workloadFactory,
        memoryManager,
        input,
        kernel,
        boost::multi_array<T, 1>(),
        expectedOutput,
        dataLayout,
        qScale,
        qOffset,
        padLeft,
        padTop,
        padRight,
        padBottom,
        strideX,
        strideY);
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SimpleConvolution2d3x5TestCommon(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    // Use common single-batch 3-channel 16x8 image.
    armnn::TensorInfo inputDesc({1, 3, 8, 16}, ArmnnType);
    boost::multi_array<T, 4> input = MakeTensor<T, 4>(inputDesc, QuantizedVector<T>(qScale, qOffset, ConvInput3x8x16));

    // Use a 2-element batch with 3-channel 3x5 kernels.
    armnn::TensorInfo kernelDesc({2, 3, 5, 3}, ArmnnType);
    boost::multi_array<T, 4> kernel = MakeTensor<T, 4>(kernelDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            1, 1, 1,
            1, -1, 1,
            1, 1, 1,
            1, 1, 1,
            1, 1, 1,

            0, 0, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 0,

            2, 2, 2,
            2, 2, 2,
            2, 2, 2,
            2, 2, 2,
            2, 2, 2,


            0, 0, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 0,

            1, 1, 1,
            1, 1, 1,
            1, 1, 1,
            1, 1, 1,
            1, 1, 1,

            0, 0, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 0,
            0, 0, 0
        })));

    // Expected output is 2 batch elements of a 1-channel 14x4 image.
    armnn::TensorInfo outputDesc({1, 2, 4, 14}, ArmnnType);
    boost::multi_array<T, 4> expectedOutput = MakeTensor<T, 4>(outputDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24, -24,
            -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25,
            -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f,
            -23.5f, -23.5f, -23.5f,
            -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f, -23.5f,
            -23.5f, -23.5f, -23.5f,

            5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        })));

    return SimpleConvolution2dTestImpl<ArmnnType, ArmnnBType>(
        workloadFactory,
        memoryManager,
        input,
        kernel,
        GetBias2<ArmnnBType>(biasEnabled, qScale * qScale),
        expectedOutput,
        qScale,
        qOffset,
        layout);
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType,
         typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SimpleConvolution2d3x3TestCommon(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    // Use a 3x3 kernel, which exercises ArmCompute's direct convolution path.

    // Use common single-batch 3-channel 16x8 image.
    armnn::TensorInfo inputDesc({1, 3, 8, 16}, ArmnnType);
    boost::multi_array<T, 4> input = MakeTensor<T, 4>(inputDesc, QuantizedVector<T>(qScale, qOffset, ConvInput3x8x16));

    // Use a 2-element batch of 3-channel 3x3 kernels.
    armnn::TensorInfo kernelDesc({2, 3, 3, 3}, ArmnnType);
    boost::multi_array<T, 4> kernel = MakeTensor<T, 4>(kernelDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            1, 1, 1,
            1, -1, 1,
            1, 1, 1,

            0, 0, 0,
            0, 0, 0,
            0, 0, 0,

            2, 2, 2,
            2, 2, 2,
            2, 2, 2,


            0, 0, 0,
            0, 0, 0,
            0, 0, 0,

            1, 1, 1,
            1, 1, 1,
            1, 1, 1,

            0, 0, 0,
            0, 0, 0,
            0, 0, 0
        })));

    // Expected output is 1 batch of a 2-channel 14x6 image.
    armnn::TensorInfo outputDesc({1, 2, 6, 14}, ArmnnType);
    boost::multi_array<T, 4> expectedOutput = MakeTensor<T, 4>(outputDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            -15, -15, -15, -15, -15, -15, -15, -15, -15, -15, -15, -15, -15, -15,
            -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16,
            -14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,
            -14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,
            -14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,
            -14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,-14.5f,

            3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        })));

    return SimpleConvolution2dTestImpl<ArmnnType, ArmnnBType>(
        workloadFactory,
        memoryManager,
        input,
        kernel,
        GetBias2<ArmnnBType>(biasEnabled, qScale * qScale),
        expectedOutput,
        qScale,
        qOffset,
        layout);
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType,
         typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> Convolution2dAsymmetricPaddingLargerThanHalfKernelSizeTestCommon(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    const armnn::DataLayout layout,
    float qScale,
    int32_t qOffset)
{
    // Use a single-batch 1-channel 3x3 image as input.
    armnn::TensorInfo inputDesc({1, 1, 3, 3}, ArmnnType);
    boost::multi_array<T, 4> input = MakeTensor<T, 4>(inputDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            11,21,31,
            12,22,32,
            13,23,33
        })));

    // Use 1 batch of a 1-channel 2x2 kernel.
    armnn::TensorInfo kernelDesc({1, 1, 2, 2}, ArmnnType);
    boost::multi_array<T, 4> kernel = MakeTensor<T, 4>(kernelDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            -11,-21,
            -12,-22,
        })));

// Expected output is 1 batch of a 1-channel 6x8 image.
// Manually calculated like this:
//[-11*0 -21*0  -12*0 -22*0  ; -11*0  -21*0  -12*0  -22*0  ; -11*0  -21*0  -12*0  -22*0  ; -11*0  -21*0 -12*0  -22*0 ..]
//[-11*0 -21*0  -12*0 -22*11 ; -11*0  -21*0  -12*11 -22*21 ; -11*0  -21*0  -12*21 -22*31 ; -11*0  -21*0 -12*31 -22*0 ..]
//[-11*0 -21*11 -12*0 -22*12 ; -11*11 -21*21 -12*12 -22*22 ; -11*21 -21*31 -12*22 -22*32 ; -11*31 -21*0 -12*32 -22*0 ..]
//[-11*0 -21*12 -12*0 -22*13 ; -11*12 -21*22 -12*13 -22*23 ; -11*22 -21*32 -12*23 -22*33 ; -11*32 -21*0 -12*33 -22*0 ..]
//[-11*0 -21*13 -12*0 -22*0  ; -11*13 -21*23 -12*0  -22*0  ; -11*23 -21*33 -12*0  -22*0  ; -11*33 -21*0 -12*0  -22*0 ..]
//[-11*0 -21*0  -12*0 -22*0  ; -11*0  -21*0  -12*0  -22*0  ; -11*0  -21*0  -12*0  -22*0  ; -11*0  -21*0 -12*0  -22*0 ..]
//[..... .....  ..... .....  ; .....  .....  .....  .....  ; .....  .....  .....  .....  ; .....  ..... .....  ..... ..]
    armnn::TensorInfo outputDesc({1, 1, 8, 6}, ArmnnType);
    boost::multi_array<T, 4> expectedOutput = MakeTensor<T, 4>(outputDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
               0,    0,      0,    0,    0,    0,
            -242,  -594,  -934, -372,    0,    0,
            -495, -1190, -1850, -725,    0,    0,
            -538, -1256, -1916, -748,    0,    0,
            -273, -626,  -946,  -363,    0,    0,
               0,    0,     0,     0,    0,    0,
               0,    0,     0,     0,    0,    0,
               0,    0,     0,     0,    0,    0
        })));

    return SimpleConvolution2dTestImpl<ArmnnType, ArmnnBType>(
        workloadFactory,
        memoryManager,
        input,
        kernel,
        GetBias2<ArmnnBType>(false, qScale * qScale),
        expectedOutput,
        qScale,
        qOffset,
        layout,
        1,  // Padding left.
        2,  // Padding top.
        3,  // Padding right.
        4); // Padding bottom.
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType,
         typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SimpleConvolution2dAsymmetricPaddingTestCommon(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    const armnn::DataLayout layout,
    float qScale,
    int32_t qOffset)
{
    // Use a single-batch 1-channel 5x5 image as input.
    armnn::TensorInfo inputDesc({ 1, 1, 5, 5 }, ArmnnType);
    boost::multi_array<T, 4> input = MakeTensor<T, 4>(inputDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            11,21,31,41,51,
            12,22,32,42,52,
            13,23,33,43,53,
            14,24,34,44,54,
            15,25,35,45,55,
        })));

    // Use 1 batch of a 1-channel 4x4 kernel.
    armnn::TensorInfo kernelDesc({ 1, 1, 4, 4 }, ArmnnType);
    boost::multi_array<T, 4> kernel = MakeTensor<T, 4>(kernelDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            -11,-21,-31,-41,
            -12,-22,-32,-42,
            -13,-23,-33,-43,
            -14,-24,-34,-44,
        })));

    // Expected output is 1 batch of a 1-channel 5x5 image.
    armnn::TensorInfo outputDesc({ 1, 1, 5, 5 }, ArmnnType);
    std::vector<T> myVec(outputDesc.GetNumElements(), 0);
    boost::multi_array<T, 4> expectedOutput = MakeTensor<T, 4>(outputDesc, std::vector<T>(
        QuantizedVector<T>(qScale, qOffset, {
            -7140, -10580, -13940,  -9300, -5230,
            -9590, -14120, -18520, -12290, -6860,
            -9980, -14560, -18960, -12560, -7000,
            -7518, -10904, -14144,  -9318, -5152,
            -5032,  -7256,  -9376,  -6142, -3368,
        })));

    return SimpleConvolution2dTestImpl<ArmnnType, ArmnnBType>(
        workloadFactory,
        memoryManager,
        input,
        kernel,
        GetBias2<ArmnnBType>(false, qScale * qScale),
        expectedOutput,
        qScale,
        qOffset,
        layout,
        1,  // Padding left.
        1,  // Padding top.
        2,  // Padding right.
        2); // Padding bottom.
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> Convolution2d3x3DilationTestCommon(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    const std::vector<float>& inputNoQuantizedValues,
    armnn::TensorInfo& inputTensorInfo,
    const std::vector<float>& kernelNoQuantizedValues,
    armnn::TensorInfo& kernelTensorInfo,
    const std::vector<float>& outputExpectedNoQuantizedValues,
    armnn::TensorInfo& outputTensorInfo,
    uint32_t dilationX,
    uint32_t dilationY,
    armnn::DataLayout layout = armnn::DataLayout::NCHW,
    uint32_t padLeft = 0,
    uint32_t padTop = 0,
    uint32_t padRight = 0,
    uint32_t padBottom = 0,
    uint32_t strideX  = 1,
    uint32_t strideY  = 1,
    bool biasEnabled = false
)
{
    float qScale;
    int32_t qOffset;
    switch (ArmnnType)
    {
        case armnn::DataType::QuantisedAsymm8:
        {
            qScale = 0.1f;
            qOffset = 128;
            break;
        }
        case armnn::DataType::QuantisedSymm16:
        {
            qScale = 0.1f;
            qOffset = 0;
            break;
        }
        case armnn::DataType::Float32:
        default:
        {
            qScale = 0.f;
            qOffset = 0;
            break;
        }
    }

    inputTensorInfo.SetQuantizationScale(qScale);
    inputTensorInfo.SetQuantizationOffset(qOffset);
    kernelTensorInfo.SetQuantizationScale(qScale);
    kernelTensorInfo.SetQuantizationOffset(qOffset);
    outputTensorInfo.SetQuantizationScale(qScale);
    outputTensorInfo.SetQuantizationOffset(qOffset);

    auto input = MakeTensor<T, 4>(inputTensorInfo,
                                  std::vector<T>(QuantizedVector<T>(inputTensorInfo.GetQuantizationScale(),
                                                                    inputTensorInfo.GetQuantizationOffset(),
                                                                    inputNoQuantizedValues)));
    auto kernel = MakeTensor<T, 4>(kernelTensorInfo,
                                  std::vector<T>(QuantizedVector<T>(kernelTensorInfo.GetQuantizationScale(),
                                                                    kernelTensorInfo.GetQuantizationOffset(),
                                                                    kernelNoQuantizedValues)));
    auto expectedOutput = MakeTensor<T, 4>(outputTensorInfo,
                                           std::vector<T>(QuantizedVector<T>(outputTensorInfo.GetQuantizationScale(),
                                                                             outputTensorInfo.GetQuantizationOffset(),
                                                                             outputExpectedNoQuantizedValues)));

    return SimpleConvolution2dTestImpl<ArmnnType, ArmnnBType>(
            workloadFactory,
            memoryManager,
            input,
            kernel,
            GetBias2<ArmnnBType>(biasEnabled, qScale * qScale),
            expectedOutput,
            qScale,
            qOffset,
            layout,
            padLeft,
            padTop,
            padRight,
            padBottom,
            strideX,
            strideY,
            dilationX,
            dilationY);
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType, typename T>
LayerTestResult<T, 4> Convolution2d3x3Dilation3x3Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    armnn::TensorInfo inputTensorInfo({1, 1, 10, 10}, ArmnnType);
    std::vector<float> inputNoQuantizedValues =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    armnn::TensorInfo kernelTensorInfo({ 1, 1, 3, 3}, ArmnnType);
    std::vector<float> kernelNoQuantizedValues =
    {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    };

    // Since the dilation rate is 3 this will dilate the kernel to be like 7x7,
    // therefore the output will be 4x4: (I−K+2P)/S +1 => (10-7 +0)/1 +1
    armnn::TensorInfo outputTensorInfo({ 1, 1, 4, 4}, ArmnnType);
    std::vector<float> outputExpectedNoQuantizedValues =
    {
        6., 5., 5., 5.,
        6., 5., 5., 5.,
        6., 5., 5., 5.,
        3., 2., 2., 2.
    };

    return Convolution2d3x3DilationTestCommon<ArmnnType, ArmnnBType>(
            workloadFactory,
            memoryManager,
            inputNoQuantizedValues,
            inputTensorInfo,
            kernelNoQuantizedValues,
            kernelTensorInfo,
            outputExpectedNoQuantizedValues,
            outputTensorInfo,
            3,
            3,
            layout,
            biasEnabled);
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType, typename T>
LayerTestResult<T, 4> Convolution2d2x3x3Dilation3x3Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    armnn::TensorInfo inputTensorInfo({1, 2, 10, 10}, ArmnnType);
    std::vector<float> inputNoQuantizedValues =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    armnn::TensorInfo kernelTensorInfo({ 1, 2, 3, 3}, ArmnnType);
    std::vector<float> kernelNoQuantizedValues =
    {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9,

        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    };

    // Since the dilation rate is 3 this will dilate the kernel to be like 7x7,
    // therefore the output will be 4x4: (I−K+2P)/S +1 => (10-7 +0)/1 +1
    armnn::TensorInfo outputTensorInfo({ 1, 1, 4, 4}, ArmnnType);
    std::vector<float> outputExpectedNoQuantizedValues =
    {
        12., 10., 10., 10.,
        12., 10., 10., 10.,
        12., 10., 10., 10.,
         6.,  4.,  4.,  4.
    };

    return Convolution2d3x3DilationTestCommon<ArmnnType, ArmnnBType>(
            workloadFactory,
            memoryManager,
            inputNoQuantizedValues,
            inputTensorInfo,
            kernelNoQuantizedValues,
            kernelTensorInfo,
            outputExpectedNoQuantizedValues,
            outputTensorInfo,
            3,
            3,
            layout,
            biasEnabled);
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType, typename T>
LayerTestResult<T, 4> Convolution2d2x2Dilation2x2Padding2x2Stride3x3Test(
        armnn::IWorkloadFactory &workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr &memoryManager,
        bool biasEnabled,
        const armnn::DataLayout layout)
{
    armnn::TensorInfo inputTensorInfo({1, 1, 10, 10}, ArmnnType);
    std::vector<float> inputNoQuantizedValues =
    {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };

    armnn::TensorInfo kernelTensorInfo({ 1, 1, 2, 2}, ArmnnType);
    std::vector<float> kernelNoQuantizedValues =
    {
        1, 2,
        3, 4
    };

    // Since the dilation rate is 2 this will dilate the kernel to be like 3x3: d(K-1)+1 --> 2 x (2-1) + 1 = 3,
    // therefore the output will be 4x4: (I − K + 2P)/S +1 => trunc ( (10 - 3 + 2x2 ) / 3 + 1 )
    // where, dilation size = d = 2; kernel size = K = 2; input size = I = 10; padding size = P = 2; stride = S = 3
    armnn::TensorInfo outputTensorInfo({ 1, 1, 4, 4}, ArmnnType);
    std::vector<float> outputExpectedNoQuantizedValues =
    {
        4,  7,  7, 3,
        6, 10, 10, 4,
        6, 10, 10, 4,
        2,  3,  3, 1
    };
    uint32_t padLeft = 1;
    uint32_t padTop = 1;
    uint32_t padRight = 1;
    uint32_t padBottom = 1;

    return Convolution2d3x3DilationTestCommon<ArmnnType, ArmnnBType>(
            workloadFactory,
            memoryManager,
            inputNoQuantizedValues,
            inputTensorInfo,
            kernelNoQuantizedValues,
            kernelTensorInfo,
            outputExpectedNoQuantizedValues,
            outputTensorInfo,
            2,
            2,
            layout,
            padLeft,
            padTop,
            padRight,
            padBottom,
            3,
            3,
            biasEnabled
            );
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T,4> CompareConvolution2dTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    armnn::IWorkloadFactory& refWorkloadFactory)
{
    unsigned int inputHeight   = 8;
    unsigned int inputWidth    = 16;
    unsigned int inputChannels = 3;
    unsigned int inputNum      = 5;

    unsigned int kernelHeight = 3;
    unsigned int kernelWidth  = 3;

    unsigned int strideX = 2;
    unsigned int strideY = 3;
    unsigned int padX    = 1;
    unsigned int padY    = 1;

    unsigned int outputNum      = inputNum;
    unsigned int outputChannels = 2;
    unsigned int outputHeight   = (inputHeight + 2 * padY - kernelHeight + strideY) / strideY;
    unsigned int outputWidth    = (inputWidth + 2 * padX - kernelWidth + strideX) / strideX;

    armnn::TensorInfo inputTensorInfo;
    armnn::TensorInfo outputTensorInfo;
    armnn::TensorInfo kernelDesc;
    armnn::TensorInfo biasDesc;

    unsigned int inputShape[]  = {inputNum, inputChannels, inputHeight, inputWidth};
    unsigned int outputShape[] = {outputNum, outputChannels, outputHeight, outputWidth};
    unsigned int kernelShape[] = {outputChannels, inputChannels, kernelHeight, kernelWidth};
    unsigned int biasShape[]   = {outputChannels};

    inputTensorInfo = armnn::TensorInfo(4, inputShape, ArmnnType);
    outputTensorInfo = armnn::TensorInfo(4, outputShape, ArmnnType);
    kernelDesc = armnn::TensorInfo(4, kernelShape, ArmnnType);
    biasDesc = armnn::TensorInfo(1, biasShape, ArmnnType);

    LayerTestResult<T,4> ret(outputTensorInfo);

    auto input  = MakeRandomTensor<T, 4>(inputTensorInfo, 124908);
    auto kernel = MakeRandomTensor<T, 4>(kernelDesc, 891234);
    auto bias   = MakeRandomTensor<T, 1>(biasDesc, 1028);

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::Convolution2dQueueDescriptor data;
    armnn::WorkloadInfo info;
    armnn::ScopedCpuTensorHandle weightsTensor(kernelDesc);
    armnn::ScopedCpuTensorHandle biasTensor(biasDesc);

    AllocateAndCopyDataToITensorHandle(&weightsTensor, &kernel[0][0][0][0]);
    AllocateAndCopyDataToITensorHandle(&biasTensor, &bias[0]);

    AddInputToWorkload(data, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());
    data.m_Weight = &weightsTensor;
    data.m_Bias = &biasTensor;
    data.m_Parameters.m_StrideX = strideX;
    data.m_Parameters.m_StrideY = strideY;
    data.m_Parameters.m_PadLeft = padX;
    data.m_Parameters.m_PadRight = padX;
    data.m_Parameters.m_PadTop = padY;
    data.m_Parameters.m_PadBottom = padY;
    data.m_Parameters.m_BiasEnabled = true;

    std::unique_ptr<armnn::ITensorHandle> outputHandleRef = refWorkloadFactory.CreateTensorHandle(outputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> inputHandleRef = refWorkloadFactory.CreateTensorHandle(inputTensorInfo);

    armnn::Convolution2dQueueDescriptor refData = data;
    armnn::WorkloadInfo               refInfo = info;
    SetWorkloadInput(refData, refInfo, 0, inputTensorInfo, inputHandleRef.get());
    SetWorkloadOutput(refData, refInfo, 0, outputTensorInfo, outputHandleRef.get());

    std::unique_ptr<armnn::IWorkload> workload  = workloadFactory.CreateConvolution2d(data, info);
    std::unique_ptr<armnn::IWorkload> workloadRef = refWorkloadFactory.CreateConvolution2d(refData, refInfo);

    outputHandleRef->Allocate();
    inputHandleRef->Allocate();

    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);
    CopyDataToITensorHandle(inputHandleRef.get(), &input[0][0][0][0]);

    ExecuteWorkload(*workload, memoryManager);

    workloadRef->PostAllocationConfigure();
    workloadRef->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());
    CopyDataFromITensorHandle(&ret.outputExpected[0][0][0][0], outputHandleRef.get());

    return ret;
}

//
// DepthwiseConvolution2d implementations
//

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType,
         typename T = armnn::ResolveType<ArmnnType>, typename B = armnn::ResolveType<ArmnnBType>>
LayerTestResult<T, 4> DepthwiseConvolution2dAsymmetricTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    const boost::multi_array<T, 4>& input,
    const boost::multi_array<T, 4>& kernel,
    const boost::multi_array<B, 1>& bias,
    const boost::multi_array<T, 4>& outputExpected,
    float qScale,
    int32_t qOffset,
    const armnn::DataLayout layout,
    uint32_t padLeft = 0,
    uint32_t padTop = 0,
    uint32_t padRight = 0,
    uint32_t padBottom = 0,
    uint32_t strideX = 1,
    uint32_t strideY = 1)
{
    unsigned int inputNum       = boost::numeric_cast<unsigned int>(input.shape()[0]);
    unsigned int inputChannels  = boost::numeric_cast<unsigned int>(input.shape()[1]);
    unsigned int inputHeight    = boost::numeric_cast<unsigned int>(input.shape()[2]);
    unsigned int inputWidth     = boost::numeric_cast<unsigned int>(input.shape()[3]);
    unsigned int kernelChanMul  = boost::numeric_cast<unsigned int>(kernel.shape()[0]);
    unsigned int kernelChannels = boost::numeric_cast<unsigned int>(kernel.shape()[1]);
    unsigned int kernelHeight   = boost::numeric_cast<unsigned int>(kernel.shape()[2]);
    unsigned int kernelWidth    = boost::numeric_cast<unsigned int>(kernel.shape()[3]);
    unsigned int outputNum      = boost::numeric_cast<unsigned int>(outputExpected.shape()[0]);
    unsigned int outputChannels = boost::numeric_cast<unsigned int>(outputExpected.shape()[1]);
    unsigned int outputHeight   = boost::numeric_cast<unsigned int>(outputExpected.shape()[2]);
    unsigned int outputWidth    = boost::numeric_cast<unsigned int>(outputExpected.shape()[3]);

    // If a bias is used, its size must equal the number of output channels.
    bool biasEnabled = bias.size() > 0;
    BOOST_ASSERT(!biasEnabled || bias.size() == outputChannels);

    // Creates the tensors.
    armnn::TensorInfo inputTensorInfo =
            armnnUtils::GetTensorInfo(inputNum, inputChannels, inputHeight, inputWidth, layout, ArmnnType);
    armnn::TensorInfo outputTensorInfo =
            armnnUtils::GetTensorInfo(outputNum, outputChannels, outputHeight, outputWidth, layout, ArmnnType);
    armnn::TensorInfo kernelDesc({kernelChanMul, kernelChannels, kernelHeight, kernelWidth}, ArmnnType);
    armnn::TensorInfo biasDesc({static_cast<unsigned int>(bias.size())}, ArmnnBType);

    // Set quantization parameters if the requested type is a quantized type.
    if (armnn::IsQuantizedType<T>())
    {
        inputTensorInfo.SetQuantizationScale(qScale);
        inputTensorInfo.SetQuantizationOffset(qOffset);
        outputTensorInfo.SetQuantizationScale(qScale);
        outputTensorInfo.SetQuantizationOffset(qOffset);
        kernelDesc.SetQuantizationScale(qScale);
        kernelDesc.SetQuantizationOffset(qOffset);
        biasDesc.SetQuantizationScale(qScale*qScale);
        biasDesc.SetQuantizationOffset(0);
    }

    // Construct the input data.
    std::vector<T> inputData;
    inputData.assign(input.data(), input.data() + inputChannels*inputHeight*inputWidth);

    // At this point if we require it permute the input data
    const armnn::PermutationVector NCHWToNHWC = { 0, 3, 1, 2 };
    if (layout == armnn::DataLayout::NHWC)
    {
        std::vector<T> tmp(inputData.size());
        armnnUtils::Permute(inputTensorInfo.GetShape(), NCHWToNHWC, inputData.data(), tmp.data(), sizeof(T));
        inputData = tmp;
    }

    auto batchedInput = MakeTensor<T, 4>(inputTensorInfo, inputData);

    // Construct the output data, with bias applied, as appropriate.
    std::vector<T> outputData;
    outputData.assign(outputExpected.data(), outputExpected.data() + outputChannels*outputHeight*outputWidth);
    if (biasEnabled)
    {
        std::vector<T> biasV;
        biasV.assign(bias.data(), bias.data() + outputChannels);
        ApplyBias(outputData, outputTensorInfo.GetQuantizationScale(), outputTensorInfo.GetQuantizationOffset(),
            biasV, biasDesc.GetQuantizationScale(), biasDesc.GetQuantizationOffset(),
            outputWidth, outputHeight);
    }

    LayerTestResult<T, 4> ret(outputTensorInfo);

    // At this point if we require it permute the expected output
    if (layout == armnn::DataLayout::NHWC)
    {
        std::vector<T> tmp(outputData.size());
        armnnUtils::Permute(outputTensorInfo.GetShape(), NCHWToNHWC, outputData.data(), tmp.data(), sizeof(T));
        outputData = tmp;
    }

    ret.outputExpected = MakeTensor<T, 4>(outputTensorInfo, outputData);

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ScopedCpuTensorHandle weightsTensor(kernelDesc);

    AllocateAndCopyDataToITensorHandle(&weightsTensor, &kernel[0][0][0][0]);

    armnn::ScopedCpuTensorHandle biasTensor(biasDesc);
    if (biasEnabled)
    {
        AllocateAndCopyDataToITensorHandle(&biasTensor, &bias[0]);
    }

    armnn::DepthwiseConvolution2dQueueDescriptor data;
    data.m_Weight = &weightsTensor;
    data.m_Bias = &biasTensor; // Still set this whether or not bias is enabled - it can be a source of bugs.
    data.m_Parameters.m_StrideX = strideX;
    data.m_Parameters.m_StrideY = strideY;
    data.m_Parameters.m_PadLeft = padLeft;
    data.m_Parameters.m_PadRight = padRight;
    data.m_Parameters.m_PadTop = padTop;
    data.m_Parameters.m_PadBottom = padBottom;
    data.m_Parameters.m_BiasEnabled = biasEnabled;
    data.m_Parameters.m_DataLayout = layout;

    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateDepthwiseConvolution2d(data, info);
    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), &batchedInput[0][0][0][0]);

    ExecuteWorkload(*workload, memoryManager);

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());

    return ret;
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> DepthwiseConvolution2dDepthMul1TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    using B = armnn::ResolveType<ArmnnBType>;

    unsigned int inputHeight = 3;
    unsigned int inputWidth = 3;
    unsigned int inputChannels = 2;
    unsigned int inputNum = 1;

    unsigned int kernelHeight = 3;
    unsigned int kernelWidth = 3;
    unsigned int kernelChannels = inputChannels;
    unsigned int kernelDepthMultiplier = 1;

    unsigned int outputHeight = 1;
    unsigned int outputWidth = 1;
    unsigned int outputChannels = kernelChannels;
    unsigned int outputNum = inputNum;

    armnn::TensorInfo inputTensorInfo =
            armnnUtils::GetTensorInfo(inputNum, inputChannels, inputHeight, inputWidth, layout, ArmnnType);
    armnn::TensorInfo outputTensorInfo =
            armnnUtils::GetTensorInfo(outputNum, outputChannels, outputHeight, outputWidth, layout, ArmnnType);
    armnn::TensorInfo kernelDesc({kernelDepthMultiplier, kernelChannels, kernelHeight, kernelWidth},
                                 ArmnnType);
    armnn::TensorInfo biasDesc({ outputChannels }, ArmnnBType);

    // Set quantization parameters if the requested type is a quantized type.
    if(armnn::IsQuantizedType<T>())
    {
        inputTensorInfo.SetQuantizationScale(qScale);
        inputTensorInfo.SetQuantizationOffset(qOffset);
        outputTensorInfo.SetQuantizationScale(qScale);
        outputTensorInfo.SetQuantizationOffset(qOffset);
        kernelDesc.SetQuantizationScale(qScale);
        kernelDesc.SetQuantizationOffset(qOffset);
        biasDesc.SetQuantizationScale(qScale*qScale);
        biasDesc.SetQuantizationOffset(0);
    }
    std::vector<T> inputData = std::vector<T>(
            QuantizedVector<T>(inputTensorInfo.GetQuantizationScale(), inputTensorInfo.GetQuantizationOffset(), {
                    1.f, 2.f, 1.f,
                    2.f, 1.f, 2.f,
                    1.f, 2.f, 1.f,

                    1.f, 2.f, 1.f,
                    2.f, 1.f, 2.f,
                    1.f, 2.f, 1.f,
            }));
    // at this point if we require it permute the input data
    const armnn::PermutationVector NCHWToNHWC = { 0, 3, 1, 2 };
    if (layout == armnn::DataLayout::NHWC)
    {
        std::vector<T> tmp(inputData.size());
        armnnUtils::Permute(inputTensorInfo.GetShape(), NCHWToNHWC, inputData.data(), tmp.data(), sizeof(T));
        inputData = tmp;
    }
    auto input = MakeTensor<T, 4>(inputTensorInfo, inputData);

    std::vector<B> biasV(QuantizedVector<B>(biasDesc.GetQuantizationScale(), biasDesc.GetQuantizationOffset(),
                                            {0, 2}));
    auto bias = MakeTensor<B, 1>(biasDesc, biasV);

    std::vector<T> kernelData = std::vector<T>(
            QuantizedVector<T>(kernelDesc.GetQuantizationScale(), kernelDesc.GetQuantizationOffset(), {
                    1.f, 0.f,  1.f,
                    0.f, 0.f,  0.f,
                    -1.f, 0.f, -1.f,

                    1.f, 0.f,  1.f,
                    0.f, 0.f,  0.f,
                    -1.f, 0.f, -1.f,
            }));
    auto kernel = MakeTensor<T, 4>(kernelDesc, kernelData);

    // Manually calculated.
    std::vector<T> outputImage(
        QuantizedVector<T>(outputTensorInfo.GetQuantizationScale(),
                           outputTensorInfo.GetQuantizationOffset(),
                           {0.f, 0.f})
    );

    // Optionally apply bias to output image.
    if(biasEnabled)
    {
        ApplyBias(outputImage, outputTensorInfo.GetQuantizationScale(), outputTensorInfo.GetQuantizationOffset(),
                  biasV, biasDesc.GetQuantizationScale(), biasDesc.GetQuantizationOffset(),
                  outputWidth, outputHeight);
    }

    LayerTestResult<T, 4> ret(outputTensorInfo);
    if (layout == armnn::DataLayout::NHWC)
    {
        std::vector<T> tmp(outputImage.size());
        armnnUtils::Permute(outputTensorInfo.GetShape(), NCHWToNHWC, outputImage.data(), tmp.data(), sizeof(T));
        outputImage = tmp;
    }

    ret.outputExpected = MakeTensor<T, 4>(outputTensorInfo, outputImage);

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::DepthwiseConvolution2dQueueDescriptor data;
    armnn::WorkloadInfo info;
    armnn::ScopedCpuTensorHandle weightsTensor(kernelDesc);
    armnn::ScopedCpuTensorHandle biasTensor(biasDesc);

    AllocateAndCopyDataToITensorHandle(&weightsTensor, &kernel[0][0][0][0]);
    AllocateAndCopyDataToITensorHandle(&biasTensor, &bias[0]);

    AddInputToWorkload(data, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    data.m_Weight = &weightsTensor;
    data.m_Bias = &biasTensor; // Still set this whether or not bias is enabled.
    data.m_Parameters.m_StrideX = 1;
    data.m_Parameters.m_StrideY = 1;
    data.m_Parameters.m_PadLeft = 0;
    data.m_Parameters.m_PadRight = 0;
    data.m_Parameters.m_PadTop = 0;
    data.m_Parameters.m_PadBottom = 0;
    data.m_Parameters.m_BiasEnabled = biasEnabled;
    data.m_Parameters.m_DataLayout = layout;

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateDepthwiseConvolution2d(data, info);
    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    ExecuteWorkload(*workload, memoryManager);

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());

    return ret;
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> DepthwiseConvolution2dTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    using B = armnn::ResolveType<ArmnnBType>;

    unsigned int depthMultiplier = 2;

    unsigned int inputHeight    = 8;
    unsigned int inputWidth     = 16;
    unsigned int inputChannels  = 2;
    unsigned int inputBatchSize = 1;

    unsigned int kernelHeight = 5;
    unsigned int kernelWidth  = 3;

    unsigned int outputHeight    = inputHeight - kernelHeight + 1 + 2;
    unsigned int outputWidth     = (inputWidth - kernelWidth + 1)/2;
    unsigned int outputChannels  = inputChannels * depthMultiplier;
    unsigned int outputBatchSize = inputBatchSize;

    armnn::TensorInfo inputTensorInfo = armnnUtils::GetTensorInfo(
            inputBatchSize, inputChannels, inputHeight, inputWidth, layout, ArmnnType);
    armnn::TensorInfo outputTensorInfo = armnnUtils::GetTensorInfo(
            outputBatchSize, outputChannels, outputHeight, outputWidth, layout, ArmnnType);
    armnn::TensorInfo kernelDesc({depthMultiplier, inputChannels, kernelHeight, kernelWidth},
                                 ArmnnType);
    armnn::TensorInfo biasDesc({outputChannels}, ArmnnBType);

    // Set quantization parameters if the requested type is a quantized type.
    if(armnn::IsQuantizedType<T>())
    {
        inputTensorInfo.SetQuantizationScale(qScale);
        inputTensorInfo.SetQuantizationOffset(qOffset);
        outputTensorInfo.SetQuantizationScale(qScale);
        outputTensorInfo.SetQuantizationOffset(qOffset);
        kernelDesc.SetQuantizationScale(qScale);
        kernelDesc.SetQuantizationOffset(qOffset);
        biasDesc.SetQuantizationScale(qScale*qScale);
        biasDesc.SetQuantizationOffset(0);
    }

    // NOTE: originalInputData is in NCHW format
    std::vector<T> originalInputData = std::vector<T>(
            QuantizedVector<T>(inputTensorInfo.GetQuantizationScale(), inputTensorInfo.GetQuantizationOffset(), {
                    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            }));
    std::vector<T> inputData = originalInputData;
    // at this point if we require it permute the input data
    const armnn::PermutationVector NCHWToNHWC = { 0, 3, 1, 2 };
    if (layout == armnn::DataLayout::NHWC)
    {
        armnnUtils::Permute(inputTensorInfo.GetShape(), NCHWToNHWC,
                            originalInputData.data(), inputData.data(), sizeof(T));
    }
    auto input = MakeTensor<T, 4>(inputTensorInfo, inputData);

    std::vector<B> biasV(QuantizedVector<B>(biasDesc.GetQuantizationScale(), biasDesc.GetQuantizationOffset(),
        {0, 2, 1, -1}));
    auto bias = MakeTensor<B, 1>(biasDesc, biasV);

    std::vector<T> kernelData = std::vector<T>(
            QuantizedVector<T>(kernelDesc.GetQuantizationScale(), kernelDesc.GetQuantizationOffset(), {
                    1, 1, 1,
                    1, -1, 1,
                    1, 1, 1,
                    1, 1, 1,
                    1, 1, 1,

                    2, 2, 2,
                    2, 2, 2,
                    2, 2, 2,
                    2, 2, 2,
                    2, 2, 2,

                    0, 0, 0,
                    0, -1, 0,
                    0, 0, 0,
                    0, 0, 0,
                    0, 0, 0,

                    0, 0, 0,
                    0, 0, 0,
                    0, 1, 0,
                    0, 0, 0,
                    0, 0, 0

            }));
    auto kernel = MakeTensor<T, 4>(kernelDesc, kernelData);

    // Manually calculated.
    std::vector<T> originalOutputImage = std::vector<T>(
        QuantizedVector<T>(outputTensorInfo.GetQuantizationScale(), outputTensorInfo.GetQuantizationOffset(), {
            3.5f,  3.5f,  3.5f,  3.5f,  3.5f,  3.5f,  3.5f,
            6.0f,  6.0f,  6.0f,  6.0f,  6.0f,  6.0f,  6.0f,
            5.0f,  5.0f,  5.0f,  5.0f,  5.0f,  5.0f,  5.0f,
            6.5f,  6.5f,  6.5f,  6.5f,  6.5f,  6.5f,  6.5f,
            6.5f,  6.5f,  6.5f,  6.5f,  6.5f,  6.5f,  6.5f,
            5.0f,  5.0f,  5.0f,  5.0f,  5.0f,  5.0f,  5.0f,

            -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
            0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
            -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
            -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
            -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,

            8.0f,  8.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            10.0f, 10.0f, 0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            10.0f, 10.0f, 0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            10.0f, 10.0f, 0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            10.0f, 10.0f, 0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            8.0f,  8.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,

            0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f
        }));

    // Optionally apply bias to output image.
    if(biasEnabled)
    {
        ApplyBias(originalOutputImage,
                  outputTensorInfo.GetQuantizationScale(),
                  outputTensorInfo.GetQuantizationOffset(),
                  biasV,
                  biasDesc.GetQuantizationScale(),
                  biasDesc.GetQuantizationOffset(),
                  outputWidth,
                  outputHeight);
    }

    LayerTestResult<T, 4> ret(outputTensorInfo);
    std::vector<T> outputImage = originalOutputImage;
    if (layout == armnn::DataLayout::NHWC)
    {
        armnnUtils::Permute(outputTensorInfo.GetShape(), NCHWToNHWC,
                            originalOutputImage.data(), outputImage.data(), sizeof(T));
    }

    ret.outputExpected = MakeTensor<T, 4>(outputTensorInfo, outputImage);

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::DepthwiseConvolution2dQueueDescriptor data;
    armnn::WorkloadInfo info;
    armnn::ScopedCpuTensorHandle weightsTensor(kernelDesc);
    armnn::ScopedCpuTensorHandle biasTensor(biasDesc);

    AllocateAndCopyDataToITensorHandle(&weightsTensor, &kernel[0][0][0][0]);
    AllocateAndCopyDataToITensorHandle(&biasTensor, &bias[0]);

    AddInputToWorkload(data, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    data.m_Weight = &weightsTensor;
    data.m_Bias = &biasTensor; // Still set this whether or not bias is enabled.
    data.m_Parameters.m_StrideX = 2;
    data.m_Parameters.m_StrideY = 1;
    data.m_Parameters.m_PadLeft = 0;
    data.m_Parameters.m_PadRight = 0;
    data.m_Parameters.m_PadTop = 1;
    data.m_Parameters.m_PadBottom = 1;
    data.m_Parameters.m_BiasEnabled = biasEnabled;
    data.m_Parameters.m_DataLayout = layout;

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateDepthwiseConvolution2d(data, info);
    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    ExecuteWorkload(*workload, memoryManager);

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());

    return ret;
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType,
        typename T = armnn::ResolveType<ArmnnType>, typename B = armnn::ResolveType<ArmnnBType>>
LayerTestResult<T, 4> DepthwiseConvolution2dTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    const boost::multi_array<T, 4>& originalInput,
    const boost::multi_array<T, 4>& originalKernel,
    const boost::multi_array<B, 1>& bias,
    const boost::multi_array<T, 4>& originalOutputExpected,
    float qScale,
    int32_t qOffset,
    const armnn::DataLayout layout = armnn::DataLayout::NCHW,
    uint32_t padLeft = 0,
    uint32_t padTop = 0,
    uint32_t padRight = 0,
    uint32_t padBottom = 0,
    uint32_t strideX = 1,
    uint32_t strideY = 1,
    uint32_t dilationX = 1,
    uint32_t dilationY = 1)
{
    unsigned int inputHeight   = boost::numeric_cast<unsigned int>(originalInput.shape()[2]);
    unsigned int inputWidth    = boost::numeric_cast<unsigned int>(originalInput.shape()[3]);
    unsigned int inputChannels = boost::numeric_cast<unsigned int>(originalInput.shape()[1]);
    unsigned int inputNum      = boost::numeric_cast<unsigned int>(originalInput.shape()[0]);

    unsigned int outputHeight   = boost::numeric_cast<unsigned int>(originalOutputExpected.shape()[2]);
    unsigned int outputWidth    = boost::numeric_cast<unsigned int>(originalOutputExpected.shape()[3]);
    unsigned int outputChannels = boost::numeric_cast<unsigned int>(originalOutputExpected.shape()[1]);
    unsigned int outputNum      = boost::numeric_cast<unsigned int>(originalOutputExpected.shape()[0]);

    unsigned int kernelHeight = boost::numeric_cast<unsigned int>(originalKernel.shape()[2]);
    unsigned int kernelWidth = boost::numeric_cast<unsigned int>(originalKernel.shape()[3]);
    unsigned int kernelChannels = boost::numeric_cast<unsigned int>(originalKernel.shape()[1]);
    unsigned int kernelDepthMul = boost::numeric_cast<unsigned int>(originalKernel.shape()[0]);

    bool biasEnabled = bias.size() > 0;

    // This function currently assumes 1 batch of input/output (and duplicates this into 2 batches).
    BOOST_ASSERT(inputNum == 1);
    BOOST_ASSERT(outputNum == 1);

    // If a bias is used, its size must equal the number of output channels.
    BOOST_ASSERT(!biasEnabled || bias.size() == outputChannels);


    // Note these tensors will use two (identical) batches.
    armnn::TensorInfo inputTensorInfo =
            armnnUtils::GetTensorInfo(2*inputNum, inputChannels, inputHeight, inputWidth, layout, ArmnnType);
    armnn::TensorInfo outputTensorInfo =
            armnnUtils::GetTensorInfo(2*outputNum, outputChannels, outputHeight, outputWidth, layout, ArmnnType);

    // Kernel must be NCHW layout always, independently of the layout of the input and output for depthwise convolution.
    armnn::TensorInfo kernelDesc({kernelDepthMul, kernelChannels, kernelHeight, kernelWidth}, ArmnnType);

    armnn::TensorInfo biasDesc({static_cast<unsigned int>(bias.size())}, ArmnnBType);

    // Set quantization parameters if the requested type is a quantized type.
    if(armnn::IsQuantizedType<T>())
    {
        inputTensorInfo.SetQuantizationScale(qScale);
        inputTensorInfo.SetQuantizationOffset(qOffset);
        outputTensorInfo.SetQuantizationScale(qScale);
        outputTensorInfo.SetQuantizationOffset(qOffset);
        kernelDesc.SetQuantizationScale(qScale);
        kernelDesc.SetQuantizationOffset(qOffset);
        biasDesc.SetQuantizationScale(qScale*qScale);
        biasDesc.SetQuantizationOffset(0);
    }

    LayerTestResult<T, 4> ret(outputTensorInfo);

    // Construct input data
    std::vector<T> input;
    input.assign(originalInput.data(), originalInput.data() + 1*inputChannels*inputHeight*inputWidth);
    std::vector<T> inputData;
    inputData.insert(inputData.end(), input.begin(), input.end());
    inputData.insert(inputData.end(), input.begin(), input.end());

    // at this point if we require it permute the input data
    const armnn::PermutationVector NCHWToNHWC = { 0, 3, 1, 2 };
    if (layout == armnn::DataLayout::NHWC)
    {
        std::vector<T> tmp(inputData.size());
        armnnUtils::Permute(inputTensorInfo.GetShape(), NCHWToNHWC, inputData.data(), tmp.data(), sizeof(T));
        inputData = tmp;
    }

    auto batchedInput = MakeTensor<T, 4>(inputTensorInfo, inputData);

    std::vector<T> output;
    output.assign(originalOutputExpected.data(),
                       originalOutputExpected.data() + outputChannels*outputHeight*outputWidth);

    // Apply bias to output data if it is enabled.
    if(biasEnabled)
    {
        std::vector<T> biasV;
        biasV.assign(bias.data(), bias.data() + outputChannels);
        ApplyBias(output, outputTensorInfo.GetQuantizationScale(), outputTensorInfo.GetQuantizationOffset(),
                  biasV, biasDesc.GetQuantizationScale(), biasDesc.GetQuantizationOffset(),
                  outputWidth, outputHeight);
    }

    // Construct expected output data
    std::vector<T> outputData;
    outputData.insert(outputData.end(), output.begin(), output.end());
    outputData.insert(outputData.end(), output.begin(), output.end());

    // at this point if we require it permute the expected output
    if (layout == armnn::DataLayout::NHWC)
    {
        std::vector<T> tmp(outputData.size());
        armnnUtils::Permute(outputTensorInfo.GetShape(), NCHWToNHWC, outputData.data(), tmp.data(), sizeof(T));
        outputData = tmp;
    }
    ret.outputExpected = MakeTensor<T, 4>(outputTensorInfo, outputData);

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::DepthwiseConvolution2dQueueDescriptor data;
    armnn::WorkloadInfo info;
    armnn::ScopedCpuTensorHandle weightsTensor(kernelDesc);
    armnn::ScopedCpuTensorHandle biasTensor(biasDesc);

    boost::multi_array<T, 4> kernel = boost::multi_array<T, 4>(originalKernel);
    AllocateAndCopyDataToITensorHandle(&weightsTensor, &kernel[0][0][0][0]);

    if(biasEnabled)
    {
        AllocateAndCopyDataToITensorHandle(&biasTensor, &bias[0]);
    }

    AddInputToWorkload(data, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    data.m_Weight = &weightsTensor;
    data.m_Bias = &biasTensor; // Still set this whether or not bias is enabled - can be a source of bugs.
    data.m_Parameters.m_StrideX = strideX;
    data.m_Parameters.m_StrideY = strideY;
    data.m_Parameters.m_PadLeft = padLeft;
    data.m_Parameters.m_PadRight = padRight;
    data.m_Parameters.m_PadTop = padTop;
    data.m_Parameters.m_PadBottom = padBottom;
    data.m_Parameters.m_BiasEnabled = biasEnabled;
    data.m_Parameters.m_DataLayout = layout;
    data.m_Parameters.m_DilationX = dilationX;
    data.m_Parameters.m_DilationY = dilationY;

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateDepthwiseConvolution2d(data, info);
    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), &batchedInput[0][0][0][0]);

    ExecuteWorkload(*workload, memoryManager);

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());

    return ret;
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType,
         typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> DepthwiseConvolution2dAsymmetricTestCommon(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    // Use a single-batch 2-channel 5x5 image as input.
    armnn::TensorInfo inputTensorInfo({ 1, 2, 5, 5 }, ArmnnType);
    auto input = MakeTensor<T, 4>(inputTensorInfo, std::vector<T>(
        QuantizedVector<T>(inputTensorInfo.GetQuantizationScale(), inputTensorInfo.GetQuantizationOffset(),
        {
             0,  1,  2,  3,  4,
             5,  6,  7,  8,  9,
            10, 11, 12, 13, 14,
            15, 16, 17, 18, 19,
            20, 21, 22, 23, 24,

            25, 26, 27, 28, 29,
            30, 31, 32, 33, 34,
            35, 36, 37, 38, 39,
            40, 41, 42, 43, 44,
            45, 46, 47, 48, 49
        })));

    // Use a depth multiplier of 1 on a 2-channel 4x4 kernel.
    armnn::TensorInfo kernelTensorInfo({ 1, 2, 4, 4 }, ArmnnType);
    auto kernel = MakeTensor<T, 4>(kernelTensorInfo, std::vector<T>(
        QuantizedVector<T>(kernelTensorInfo.GetQuantizationScale(), kernelTensorInfo.GetQuantizationOffset(),
        {
            32, 31, 30, 29,
            28, 27, 26, 25,
            24, 23, 22, 21,
            20, 19, 18, 17,

            16, 15, 14, 13,
            12, 11, 10,  9,
             8,  7,  6,  5,
             4,  3,  2,  1
        })));

    // Expected output is 1 batch of a 2-channel 5x5 image.
    // Calculated using the python tensorflow library with strideX=1, strideY=1.
    armnn::TensorInfo outputTensorInfo({ 1, 2, 5, 5 }, ArmnnType);
    boost::multi_array<T, 4> expectedOutput = MakeTensor<T, 4>(outputTensorInfo, std::vector<T>(
        QuantizedVector<T>(outputTensorInfo.GetQuantizationScale(), outputTensorInfo.GetQuantizationOffset(),
        {
            1062, 1580, 1850, 1530, 1117,
            2140, 3108, 3500, 2842, 2042,
            3580, 5068, 5460, 4342, 3062,
            3618, 5072, 5390, 4248, 2971,
            3074, 4282, 4510, 3533, 2457,

            1550, 2284, 2362, 1955, 1428,
            2910, 4206, 4342, 3528, 2536,
            3390, 4886, 5022, 4068, 2916,
            3566, 5056, 5182, 4133, 2922,
            3100, 4352, 4452, 3517, 2465
        })));

    return DepthwiseConvolution2dAsymmetricTestImpl<ArmnnType, ArmnnBType>(
        workloadFactory,
        memoryManager,
        input,
        kernel,
        GetBias2<ArmnnBType>(biasEnabled, qScale * qScale),
        expectedOutput,
        qScale,
        qOffset,
        layout,
        1,  // Padding left.
        1,  // Padding top.
        2,  // Padding right.
        2,  // Padding bottom.
        1,  // strideX
        1); // strideY
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType,
         typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> DepthwiseConvolution2dNhwcTestCommon(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset,
    bool biasEnabled)
{
    auto layout = armnn::DataLayout::NHWC;

    armnn::TensorInfo inputTensorInfo({ 1, 2, 5, 5}, ArmnnType);
    auto input = MakeTensor<T, 4>(inputTensorInfo, std::vector<T>(
        QuantizedVector<T>(inputTensorInfo.GetQuantizationScale(), inputTensorInfo.GetQuantizationOffset(),
        {
             0,  1,  2,  3,  4,
             5,  6,  7,  8,  9,
            10, 11, 12, 13, 14,
            15, 16, 17, 18, 19,
            20, 21, 22, 23, 24,

            25, 26, 27, 28, 29,
            30, 31, 32, 33, 34,
            35, 36, 37, 38, 39,
            40, 41, 42, 43, 44,
            45, 46, 47, 48, 49
        })));

    armnn::TensorInfo kernelTensorInfo({ 1, 2, 4, 4 }, ArmnnType);
    auto kernel = MakeTensor<T, 4>(kernelTensorInfo, std::vector<T>(
        QuantizedVector<T>(kernelTensorInfo.GetQuantizationScale(), kernelTensorInfo.GetQuantizationOffset(),
        {
             32, 31, 30, 29,
             28, 27, 26, 25,
             24, 23, 22, 21,
             20, 19, 18, 17,

             16, 15, 14, 13,
             12, 11, 10,  9,
              8,  7,  6,  5,
              4,  3,  2,  1
        })));

    armnn::TensorInfo outputTensorInfo({ 1, 2, 5, 5}, ArmnnType);
    boost::multi_array<T, 4> expectedOutput = MakeTensor<T, 4>(outputTensorInfo, std::vector<T>(
        QuantizedVector<T>(outputTensorInfo.GetQuantizationScale(), outputTensorInfo.GetQuantizationOffset(),
        {
            1062, 1580, 1850, 1530, 1117,
            2140, 3108, 3500, 2842, 2042,
            3580, 5068, 5460, 4342, 3062,
            3618, 5072, 5390, 4248, 2971,
            3074, 4282, 4510, 3533, 2457,

            1550, 2284, 2362, 1955, 1428,
            2910, 4206, 4342, 3528, 2536,
            3390, 4886, 5022, 4068, 2916,
            3566, 5056, 5182, 4133, 2922,
            3100, 4352, 4452, 3517, 2465
        })));

    return DepthwiseConvolution2dTestImpl<ArmnnType, ArmnnBType>(
        workloadFactory,
        memoryManager,
        input,
        kernel,
        GetBias2<ArmnnBType>(biasEnabled, qScale * qScale),
        expectedOutput,
        qScale,
        qOffset,
        layout,
        1,  // Padding left.
        1,  // Padding top.
        2,  // Padding right.
        2,  // Padding bottom.
        1,  // strideX
        1);  // strideY
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType,
         typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> SimpleDepthwiseConvolution2d3x3Dilation3x3NhwcTestCommon(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset,
    bool biasEnabled)
{
    auto layout = armnn::DataLayout::NHWC;

    armnn::TensorInfo inputTensorInfo({ 1, 1, 9, 9}, ArmnnType);
    auto input = MakeTensor<T, 4>(inputTensorInfo, std::vector<T>(
        QuantizedVector<T>(inputTensorInfo.GetQuantizationScale(), inputTensorInfo.GetQuantizationOffset(),
        {
             0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 1, 1, 1, 0, 0, 0,
             0, 0, 0, 1, 1, 1, 0, 0, 0,
             0, 0, 0, 1, 1, 1, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0
        })));

    armnn::TensorInfo kernelTensorInfo({ 1, 1, 3, 3}, ArmnnType);
    auto kernel = MakeTensor<T, 4>(kernelTensorInfo, std::vector<T>(
        QuantizedVector<T>(kernelTensorInfo.GetQuantizationScale(), kernelTensorInfo.GetQuantizationOffset(),
        {
             1, 2, 3,
             4, 5, 6,
             7, 8, 9
        })));

    uint32_t padLeft = 0;
    uint32_t padTop = 0;
    uint32_t padRight = 0;
    uint32_t padBottom = 0;
    uint32_t strideX  = 1;
    uint32_t strideY  = 1;
    uint32_t dilationX  = 3;
    uint32_t dilationY  = 3;

    // Since the dilation rate is 3 this will reduce the size of the output from 9x9 to 3x3 of all 5s.
    armnn::TensorInfo outputTensorInfo({ 1, 1, 3, 3}, ArmnnType);
    boost::multi_array<T, 4> expectedOutput = MakeTensor<T, 4>(outputTensorInfo, std::vector<T>(
        QuantizedVector<T>(outputTensorInfo.GetQuantizationScale(), outputTensorInfo.GetQuantizationOffset(),
        {
             5, 5, 5,
             5, 5, 5,
             5, 5, 5
        })));

    return DepthwiseConvolution2dTestImpl<ArmnnType, ArmnnBType>(
        workloadFactory,
        memoryManager,
        input,
        kernel,
        GetBias2<ArmnnBType>(biasEnabled, qScale * qScale),
        expectedOutput,
        qScale,
        qOffset,
        layout,
        padLeft,
        padTop,
        padRight,
        padBottom,
        strideX,
        strideY,
        dilationX,
        dilationY);
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> DepthwiseConvolution2d3x3DilationTestCommon(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
        const std::vector<float>& inputNoQuantizedValues,
        armnn::TensorInfo& inputTensorInfo,
        const std::vector<float>& kernelNoQuantizedValues,
        armnn::TensorInfo& kernelTensorInfo,
        const std::vector<float>& outputExpectedNoQuantizedValues,
        armnn::TensorInfo& outputTensorInfo,
        uint32_t dilationX,
        uint32_t dilationY,
        armnn::DataLayout layout = armnn::DataLayout::NCHW,
        bool biasEnabled = false)
{
    float qScale;
    int32_t qOffset;
    switch (ArmnnType)
    {
        case armnn::DataType::QuantisedAsymm8:
        {
            qScale = 0.1f;
            qOffset = 128;
            break;
        }
        case armnn::DataType::QuantisedSymm16:
        {
            qScale = 0.1f;
            qOffset = 0;
            break;
        }
        case armnn::DataType::Float32:
        default:
        {
            qScale = 0.f;
            qOffset = 0;
            break;
        }
    }

    inputTensorInfo.SetQuantizationScale(qScale);
    inputTensorInfo.SetQuantizationOffset(qOffset);
    kernelTensorInfo.SetQuantizationScale(qScale);
    kernelTensorInfo.SetQuantizationOffset(qOffset);
    outputTensorInfo.SetQuantizationScale(qScale);
    outputTensorInfo.SetQuantizationOffset(qOffset);

    auto input = MakeTensor<T, 4>(inputTensorInfo,
                                  std::vector<T>(QuantizedVector<T>(inputTensorInfo.GetQuantizationScale(),
                                                                    inputTensorInfo.GetQuantizationOffset(),
                                                                    inputNoQuantizedValues)));
    auto kernel = MakeTensor<T, 4>(kernelTensorInfo,
                                   std::vector<T>(QuantizedVector<T>(kernelTensorInfo.GetQuantizationScale(),
                                                                     kernelTensorInfo.GetQuantizationOffset(),
                                                                     kernelNoQuantizedValues)));
    auto expectedOutput = MakeTensor<T, 4>(outputTensorInfo,
                                           std::vector<T>(QuantizedVector<T>(outputTensorInfo.GetQuantizationScale(),
                                                                             outputTensorInfo.GetQuantizationOffset(),
                                                                             outputExpectedNoQuantizedValues)));

    uint32_t padLeft = 0;
    uint32_t padTop = 0;
    uint32_t padRight = 0;
    uint32_t padBottom = 0;
    uint32_t strideX  = 1;
    uint32_t strideY  = 1;

    return DepthwiseConvolution2dTestImpl<ArmnnType, ArmnnBType>(
            workloadFactory,
            memoryManager,
            input,
            kernel,
            GetBias<ArmnnBType>(biasEnabled, qScale * qScale, outputTensorInfo, layout),
            expectedOutput,
            qScale,
            qOffset,
            layout,
            padLeft,
            padTop,
            padRight,
            padBottom,
            strideX,
            strideY,
            dilationX,
            dilationY);
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType, typename T>
LayerTestResult<T, 4> DepthwiseConvolution2d3x3Dilation3x3Test(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
        bool biasEnabled,
        const armnn::DataLayout layout)
{
    armnn::TensorInfo inputTensorInfo({1, 1, 10, 10}, ArmnnType);
    std::vector<float> inputNoQuantizedValues =
            {
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
                    0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
                    0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            };

    armnn::TensorInfo kernelTensorInfo({ 1, 1, 3, 3}, ArmnnType);
    std::vector<float> kernelNoQuantizedValues =
            {
                    1, 2, 3,
                    4, 5, 6,
                    7, 8, 9
            };

    // Since the dilation rate is 3 this will dilate the kernel to be like 7x7,
    // therefore the output will be 4x4: (I−K+2P)/S +1 => (10-7 +0)/1 +1
    armnn::TensorInfo outputTensorInfo({ 1, 1, 4, 4}, ArmnnType);
    std::vector<float> outputExpectedNoQuantizedValues =
            {
                    6., 5., 5., 5.,
                    6., 5., 5., 5.,
                    6., 5., 5., 5.,
                    3., 2., 2., 2.
            };

    return DepthwiseConvolution2d3x3DilationTestCommon<ArmnnType, ArmnnBType>(
            workloadFactory,
            memoryManager,
            inputNoQuantizedValues,
            inputTensorInfo,
            kernelNoQuantizedValues,
            kernelTensorInfo,
            outputExpectedNoQuantizedValues,
            outputTensorInfo,
            3,
            3,
            layout,
            biasEnabled);
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType, typename T>
LayerTestResult<T, 4> DepthwiseConvolution2d2x3x3Dilation3x3Test(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
        bool biasEnabled,
        const armnn::DataLayout layout)
{
    armnn::TensorInfo inputTensorInfo({1, 2, 10, 10}, ArmnnType);
    std::vector<float> inputNoQuantizedValues =
            {
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
                    0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
                    0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
                    0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
                    0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            };

    armnn::TensorInfo kernelTensorInfo({ 1, 2, 3, 3}, ArmnnType);
    std::vector<float> kernelNoQuantizedValues =
            {
                    1, 2, 3,
                    4, 5, 6,
                    7, 8, 9,

                    1, 2, 3,
                    4, 5, 6,
                    7, 8, 9
            };

    // Since the dilation rate is 3 this will dilate the kernel to be like 7x7,
    // therefore the output will be 2x4x4: (I−K+2P)/S +1 => (10-7 +0)/1 +1
    armnn::TensorInfo outputTensorInfo({ 1, 2, 4, 4}, ArmnnType);
    std::vector<float> outputExpectedNoQuantizedValues =
            {
                    6., 5., 5., 5.,
                    6., 5., 5., 5.,
                    6., 5., 5., 5.,
                    3., 2., 2., 2.,

                    6., 5., 5., 5.,
                    6., 5., 5., 5.,
                    6., 5., 5., 5.,
                    3., 2., 2., 2.
            };

    return DepthwiseConvolution2d3x3DilationTestCommon<ArmnnType, ArmnnBType>(
            workloadFactory,
            memoryManager,
            inputNoQuantizedValues,
            inputTensorInfo,
            kernelNoQuantizedValues,
            kernelTensorInfo,
            outputExpectedNoQuantizedValues,
            outputTensorInfo,
            3,
            3,
            layout,
            biasEnabled);
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType, typename T>
LayerTestResult<T, 4> DepthwiseConvolution2dMult4Test(
            armnn::IWorkloadFactory& workloadFactory,
            const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
            bool biasEnabled,
            const armnn::DataLayout layout)
{
    armnn::TensorInfo inputTensorInfo({1, 2, 3, 3}, ArmnnType);
    std::vector<float> inputNoQuantizedValues =
            {
                    10.0, 10.0, 10.0,
                    10.0, 10.0, 10.0,
                    10.0, 10.0, 10.0,

                    21.0, 22.0, 23.0,
                    24.0, 25.0, 26.0,
                    27.0, 28.0, 29.0
            };

    armnn::TensorInfo kernelTensorInfo({ 4, 2, 2, 2}, ArmnnType);

    std::vector<float> kernelNoQuantizedValues =
            {
                    0.25f, 0.25f,
                    0.25f, 0.25f,

                    0.25f, 0.25f,
                    0.25f, 0.25f,

                    0.0f , 0.0f,
                    0.0f , 0.1f,

                    0.0f , 0.0f,
                    0.0f , 0.1f,

                    0.2f , 0.0f,
                    0.0f , 0.0f,

                    0.2f , 0.0f,
                    0.0f , 0.0f,

                    0.0f , 0.3f,
                    0.0f , 0.0f,

                    0.0f , 0.3f,
                    0.0f , 0.0f
            };

    armnn::TensorInfo outputTensorInfo({ 1, 8, 2, 2}, ArmnnType);
    std::vector<float> outputExpectedNoQuantizedValues =
            {
                    10.f, 10.f,
                    10.f, 10.f,

                    1.f, 1.f,
                    1.f, 1.f,

                    2.f, 2.f,
                    2.f, 2.f,

                    3.f, 3.f,
                    3.f, 3.f,

                    23.f, 24.f,
                    26.f, 27.f,

                    2.5f, 2.6000001f,
                    2.8f, 2.9f,

                    4.2000003f, 4.4f,
                    4.8f, 5.f,

                    6.6000004f, 6.9f,
                    7.5000005f, 7.8f
            };


    return DepthwiseConvolution2d3x3DilationTestCommon<ArmnnType, ArmnnBType>(
            workloadFactory,
            memoryManager,
            inputNoQuantizedValues,
            inputTensorInfo,
            kernelNoQuantizedValues,
            kernelTensorInfo,
            outputExpectedNoQuantizedValues,
            outputTensorInfo,
            1,
            1,
            layout,
            biasEnabled);
}

template<armnn::DataType ArmnnType, armnn::DataType ArmnnBType, typename T>
LayerTestResult<T, 4> DepthwiseConvolution2dMult2Test(
            armnn::IWorkloadFactory& workloadFactory,
            const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
            bool biasEnabled,
            const armnn::DataLayout layout)
{
    armnn::TensorInfo inputTensorInfo({1, 2, 3, 3}, ArmnnType);
    std::vector<float> inputNoQuantizedValues =
            {
                    10.0, 10.0, 10.0,
                    10.0, 10.0, 10.0,
                    10.0, 10.0, 10.0,

                    21.0, 22.0, 23.0,
                    24.0, 25.0, 26.0,
                    27.0, 28.0, 29.0
            };

    armnn::TensorInfo kernelTensorInfo({ 2, 2, 2, 2}, ArmnnType);

    std::vector<float> kernelNoQuantizedValues =
            {
                    0.25f, 0.25f,
                    0.25f, 0.25f,

                    0.2f , 0.0f,
                    0.0f , 0.0f,

                    0.0f , 0.0f,
                    0.0f , 0.1f,

                    0.0f , 0.3f,
                    0.0f , 0.0f

            };

    armnn::TensorInfo outputTensorInfo({ 1, 4, 2, 2}, ArmnnType);
    std::vector<float> outputExpectedNoQuantizedValues =
            {
                    10.f, 10.f,
                    10.f, 10.f,

                    1.f, 1.f,
                    1.f, 1.f,

                    4.2000003f, 4.4f,
                    4.8f, 5.f,

                    6.6000004f, 6.9f,
                    7.5000005f, 7.8f
            };


    return DepthwiseConvolution2d3x3DilationTestCommon<ArmnnType, ArmnnBType>(
            workloadFactory,
            memoryManager,
            inputNoQuantizedValues,
            inputTensorInfo,
            kernelNoQuantizedValues,
            kernelTensorInfo,
            outputExpectedNoQuantizedValues,
            outputTensorInfo,
            1,
            1,
            layout,
            biasEnabled);
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> CompareDepthwiseConvolution2dTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    armnn::IWorkloadFactory& refWorkloadFactory,
    const armnnUtils::DataLayoutIndexed& layout)
{
    unsigned int inputHeight = 8;
    unsigned int inputWidth = 16;
    unsigned int inputChannels = 3;
    unsigned int inputNum = 5;

    unsigned int kernelHeight = 3;
    unsigned int kernelWidth = 3;
    unsigned int channelMultiplier = 1;

    unsigned int strideX = 2;
    unsigned int strideY = 3;
    unsigned int padX = 1;
    unsigned int padY = 1;

    unsigned int outputNum = inputNum;
    unsigned int outputChannels = inputChannels * channelMultiplier;
    unsigned int outputHeight = (inputHeight + 2 * padY - kernelHeight + strideY) / strideY;
    unsigned int outputWidth = (inputWidth + 2 * padX - kernelWidth + strideX) / strideX;

    armnn::TensorInfo inputTensorInfo;
    armnn::TensorInfo outputTensorInfo;
    armnn::TensorInfo kernelDesc;
    armnn::TensorInfo biasDesc;


    std::vector<unsigned int> inputShape;
    std::vector<unsigned int> outputShape;
    std::vector<unsigned int> kernelShape{ channelMultiplier, inputChannels, kernelHeight, kernelWidth };
    std::vector<unsigned int> biasShape{ outputChannels };
    switch (layout.GetDataLayout())
    {
        case armnn::DataLayout::NCHW:
            inputShape =  { inputNum, inputChannels, inputHeight, inputWidth };
            outputShape = { outputNum, outputChannels, outputHeight, outputWidth };
            break;
        case armnn::DataLayout ::NHWC:
            inputShape =  { inputNum, inputHeight, inputWidth, inputChannels };
            outputShape = { outputNum, outputHeight, outputWidth, outputChannels };
            break;
        default:
            throw armnn::InvalidArgumentException("unknown data layout ["
                                                  + std::to_string(static_cast<int>(layout.GetDataLayout())) + "]");
    }

    float inputsQScale = armnn::IsQuantizedType<T>() ? 1.0f : 0;
    float outputQScale = armnn::IsQuantizedType<T>() ? 2.0f : 0;
    int32_t qOffset = 0;

    inputTensorInfo = armnn::TensorInfo(4, inputShape.data(), ArmnnType, inputsQScale, qOffset);
    outputTensorInfo = armnn::TensorInfo(4, outputShape.data(), ArmnnType, outputQScale, qOffset);
    kernelDesc = armnn::TensorInfo(4, kernelShape.data(), ArmnnType, inputsQScale, qOffset);
    biasDesc = armnn::TensorInfo(
        1, biasShape.data(), armnn::GetBiasDataType(ArmnnType), inputsQScale, qOffset);

    LayerTestResult<T, 4> ret(outputTensorInfo);

    auto input = MakeRandomTensor<T, 4>(inputTensorInfo, 124908, 0.0f, 255.0f);
    auto kernel = MakeRandomTensor<T, 4>(kernelDesc, 891234, 0.0f, 255.0f);
    auto bias = MakeRandomTensor<typename FullyConnectedBiasTypeForInputType<T>::Type, 1>(
            biasDesc, 1028, 0.0f, 255.0f);

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::DepthwiseConvolution2dQueueDescriptor data;
    armnn::WorkloadInfo info;
    armnn::ScopedCpuTensorHandle weightsTensor(kernelDesc);
    armnn::ScopedCpuTensorHandle biasTensor(biasDesc);

    AllocateAndCopyDataToITensorHandle(&weightsTensor, &kernel[0][0][0][0]);
    AllocateAndCopyDataToITensorHandle(&biasTensor, &bias[0]);

    AddInputToWorkload(data, info, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());
    data.m_Weight = &weightsTensor;
    data.m_Bias = &biasTensor;
    data.m_Parameters.m_StrideX = strideX;
    data.m_Parameters.m_StrideY = strideY;
    data.m_Parameters.m_PadLeft = padX;
    data.m_Parameters.m_PadRight = padX;
    data.m_Parameters.m_PadTop = padY;
    data.m_Parameters.m_PadBottom = padY;
    data.m_Parameters.m_BiasEnabled = true;
    data.m_Parameters.m_DataLayout = layout.GetDataLayout();

    std::unique_ptr<armnn::ITensorHandle> outputHandleRef = refWorkloadFactory.CreateTensorHandle(outputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> inputHandleRef = refWorkloadFactory.CreateTensorHandle(inputTensorInfo);

    armnn::DepthwiseConvolution2dQueueDescriptor refData = data;
    armnn::WorkloadInfo refInfo = info;
    SetWorkloadInput(refData, refInfo, 0, inputTensorInfo, inputHandleRef.get());
    SetWorkloadOutput(refData, refInfo, 0, outputTensorInfo, outputHandleRef.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateDepthwiseConvolution2d(data, info);
    std::unique_ptr<armnn::IWorkload> workloadRef = refWorkloadFactory.CreateDepthwiseConvolution2d(refData, refInfo);

    outputHandleRef->Allocate();
    inputHandleRef->Allocate();

    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);
    CopyDataToITensorHandle(inputHandleRef.get(), &input[0][0][0][0]);

    ExecuteWorkload(*workload, memoryManager);

    workloadRef->PostAllocationConfigure();
    workloadRef->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0][0], outputHandle.get());
    CopyDataFromITensorHandle(&ret.outputExpected[0][0][0][0], outputHandleRef.get());

    return ret;
}

//
// Explicit template specializations
//

template LayerTestResult<armnn::ResolveType<armnn::DataType::Float32>, 4>
Convolution2d3x3Dilation3x3Test<armnn::DataType::Float32, armnn::DataType::Float32>(
    armnn::IWorkloadFactory&,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr&,
    bool,
    armnn::DataLayout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::QuantisedAsymm8>, 4>
Convolution2d3x3Dilation3x3Test<armnn::DataType::QuantisedAsymm8, armnn::DataType::Signed32>(
    armnn::IWorkloadFactory&,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr&,
    bool,
    armnn::DataLayout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::QuantisedSymm16>, 4>
Convolution2d3x3Dilation3x3Test<armnn::DataType::QuantisedSymm16, armnn::DataType::Signed32>(
    armnn::IWorkloadFactory&,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr&,
    bool,
    armnn::DataLayout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::Float32>, 4>
Convolution2d2x3x3Dilation3x3Test<armnn::DataType::Float32, armnn::DataType::Float32>(
    armnn::IWorkloadFactory&,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr&,
    bool,
    armnn::DataLayout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::QuantisedAsymm8>, 4>
Convolution2d2x3x3Dilation3x3Test<armnn::DataType::QuantisedAsymm8, armnn::DataType::Signed32>(
    armnn::IWorkloadFactory&,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr&,
    bool,
    armnn::DataLayout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::QuantisedSymm16>, 4>
Convolution2d2x3x3Dilation3x3Test<armnn::DataType::QuantisedSymm16, armnn::DataType::Signed32>(
    armnn::IWorkloadFactory&,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr&,
    bool,
    armnn::DataLayout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::Float32>, 4>
Convolution2d2x2Dilation2x2Padding2x2Stride3x3Test<armnn::DataType::Float32, armnn::DataType::Float32>(
    armnn::IWorkloadFactory &workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr &memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::QuantisedAsymm8>, 4>
Convolution2d2x2Dilation2x2Padding2x2Stride3x3Test<armnn::DataType::QuantisedAsymm8, armnn::DataType::Signed32>(
    armnn::IWorkloadFactory &workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr &memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::QuantisedSymm16>, 4>
Convolution2d2x2Dilation2x2Padding2x2Stride3x3Test<armnn::DataType::QuantisedSymm16, armnn::DataType::Signed32>(
    armnn::IWorkloadFactory &workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr &memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::Float32>, 4>
DepthwiseConvolution2d3x3Dilation3x3Test<armnn::DataType::Float32, armnn::DataType::Float32>(
        armnn::IWorkloadFactory&,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr&,
        bool,
        armnn::DataLayout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::QuantisedAsymm8>, 4>
DepthwiseConvolution2d3x3Dilation3x3Test<armnn::DataType::QuantisedAsymm8, armnn::DataType::Signed32>(
        armnn::IWorkloadFactory&,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr&,
        bool,
        armnn::DataLayout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::QuantisedSymm16>, 4>
DepthwiseConvolution2d3x3Dilation3x3Test<armnn::DataType::QuantisedSymm16, armnn::DataType::Signed32>(
        armnn::IWorkloadFactory&,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr&,
        bool,
        armnn::DataLayout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::Float32>, 4>
DepthwiseConvolution2d2x3x3Dilation3x3Test<armnn::DataType::Float32, armnn::DataType::Float32>(
        armnn::IWorkloadFactory&,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr&,
        bool,
        armnn::DataLayout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::QuantisedAsymm8>, 4>
DepthwiseConvolution2d2x3x3Dilation3x3Test<armnn::DataType::QuantisedAsymm8, armnn::DataType::Signed32>(
        armnn::IWorkloadFactory&,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr&,
        bool,
        armnn::DataLayout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::QuantisedSymm16>, 4>
DepthwiseConvolution2d2x3x3Dilation3x3Test<armnn::DataType::QuantisedSymm16, armnn::DataType::Signed32>(
        armnn::IWorkloadFactory&,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr&,
        bool,
        armnn::DataLayout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::Float32>, 4>
DepthwiseConvolution2dMult4Test<armnn::DataType::Float32, armnn::DataType::Float32>(
        armnn::IWorkloadFactory &workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr &memoryManager,
        bool biasEnabled,
        const armnn::DataLayout layout);

template LayerTestResult<armnn::ResolveType<armnn::DataType::Float32>, 4>
DepthwiseConvolution2dMult2Test<armnn::DataType::Float32, armnn::DataType::Float32>(
        armnn::IWorkloadFactory &workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr &memoryManager,
        bool biasEnabled,
        const armnn::DataLayout layout);

//
// Implementation functions
//

LayerTestResult<float, 4> SimpleConvolution2d3x5Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    return SimpleConvolution2d3x5TestCommon<armnn::DataType::Float32, armnn::DataType::Float32>(
        workloadFactory, memoryManager, 0.f, 0, biasEnabled, layout);
}

LayerTestResult<uint8_t, 4> SimpleConvolution2d3x5Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    return SimpleConvolution2d3x5TestCommon<armnn::DataType::QuantisedAsymm8, armnn::DataType::Signed32>(
        workloadFactory, memoryManager, 0.5f, 50, biasEnabled, layout);
}

LayerTestResult<float, 4> SimpleConvolution2d3x3Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    return SimpleConvolution2d3x3TestCommon<armnn::DataType::Float32, armnn::DataType::Float32>(
        workloadFactory, memoryManager, 0.f, 0, biasEnabled, layout);
}

LayerTestResult<float, 4> SimpleConvolution2d3x3NhwcTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled)
{
    return SimpleConvolution2d3x3NhwcTestCommon<armnn::DataType::Float32>(
        workloadFactory,
        memoryManager,
        0.f,
        0,
        biasEnabled,
        armnn::DataLayout::NHWC);
}

LayerTestResult<float, 4> SimpleConvolution2d3x3Stride2x2Test(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
        bool biasEnabled,
        const armnn::DataLayout layout)
{
    return SimpleConvolution2d3x3Stride2x2TestCommon<armnn::DataType::Float32>(
        workloadFactory,
        memoryManager,
        0.f,
        0,
        biasEnabled,
        layout);
}

LayerTestResult<uint8_t, 4> SimpleConvolution2d3x3Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    return SimpleConvolution2d3x3TestCommon<armnn::DataType::QuantisedAsymm8, armnn::DataType::Signed32>(
        workloadFactory, memoryManager, 0.5f, 50, biasEnabled, layout);
}

LayerTestResult<int16_t, 4> SimpleConvolution2d3x5QSymm16Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    return SimpleConvolution2d3x5TestCommon<armnn::DataType::QuantisedSymm16, armnn::DataType::Signed32>(
        workloadFactory, memoryManager, 0.5f, 50, biasEnabled, layout);
}

LayerTestResult<int16_t, 4> SimpleConvolution2d3x3QSymm16Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    return SimpleConvolution2d3x3TestCommon<armnn::DataType::QuantisedSymm16, armnn::DataType::Signed32>(
            workloadFactory, memoryManager, 0.5f, 50, biasEnabled, layout);
}

LayerTestResult<float, 4> Convolution2dAsymmetricPaddingTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    armnn::DataLayout layout)
{
    return SimpleConvolution2dAsymmetricPaddingTestCommon<armnn::DataType::Float32, armnn::DataType::Float32>(
            workloadFactory, memoryManager, layout, 0.0f, 0);
}

LayerTestResult<float, 4> Convolution2dAsymmetricPaddingLargerThanHalfKernelSizeTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    armnn::DataLayout layout)
{
    return Convolution2dAsymmetricPaddingLargerThanHalfKernelSizeTestCommon
            <armnn::DataType::Float32, armnn::DataType::Float32>(
            workloadFactory, memoryManager, layout, 0.0f, 0);
}

LayerTestResult<float, 4> Convolution1dTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled)
{
    return Convolution1dTestImpl<armnn::DataType::Float32, armnn::DataType::Float32>(
            workloadFactory, memoryManager, 0.0f, 0, biasEnabled);
}

LayerTestResult<uint8_t, 4> Convolution1dUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled)
{
    return Convolution1dTestImpl<armnn::DataType::QuantisedAsymm8, armnn::DataType::Signed32>(
            workloadFactory, memoryManager, 0.1f, 128, biasEnabled);
}

LayerTestResult<float,4> CompareConvolution2dTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    armnn::IWorkloadFactory& refWorkloadFactory)
{
    return CompareConvolution2dTestImpl<armnn::DataType::Float32>(
            workloadFactory, memoryManager, refWorkloadFactory);
}

LayerTestResult<float, 4> DepthwiseConvolution2dTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    return DepthwiseConvolution2dTestImpl<armnn::DataType::Float32, armnn::DataType::Float32>(
        workloadFactory, memoryManager, 0.0f, 0, biasEnabled, layout);
}

LayerTestResult<float, 4> DepthwiseConvolution2dDepthNhwcTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled)
{
    return DepthwiseConvolution2dNhwcTestCommon<armnn::DataType::Float32, armnn::DataType::Float32>(
        workloadFactory, memoryManager, 0.0f, 0, biasEnabled);
}

LayerTestResult<float, 4> DepthwiseConvolution2dDepthMul1Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    return DepthwiseConvolution2dDepthMul1TestImpl<armnn::DataType::Float32, armnn::DataType::Float32>(
        workloadFactory, memoryManager, 0.0f, 0, biasEnabled, layout);
}

LayerTestResult<float, 4> DepthwiseConvolution2dDepthMul64Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    armnn::TensorInfo inputTensorInfo({ 1, 1, 2, 2 }, armnn::DataType::Float32);
    auto input = MakeTensor<float, 4>(inputTensorInfo, { 1.f, 2.f, 3.f, 4.f });

    std::vector<float> kernelData;
    std::vector<float> singleDepthKernel{ 1.f, -1.f, -1.f, 1.f };
    for (unsigned int i = 0; i < 64; ++i)
    {
        kernelData.insert(kernelData.end(), singleDepthKernel.begin(), singleDepthKernel.end());
    }
    armnn::TensorInfo kernelTensorInfo({ 64, 1, 2, 2 }, armnn::DataType::Float32);
    auto kernel = MakeTensor<float, 4>(kernelTensorInfo, kernelData);

    std::vector<float> expectedOutputData(64, 0.f);
    armnn::TensorInfo outputTensorInfo({ 1, 64, 1, 1 }, armnn::DataType::Float32);
    auto expectedOutput = MakeTensor<float, 4>(outputTensorInfo, expectedOutputData);

    return DepthwiseConvolution2dTestImpl<armnn::DataType::Float32, armnn::DataType::Float32>(
            workloadFactory,
            memoryManager,
            input,
            kernel,
            boost::multi_array<float, 1>(),
            expectedOutput,
            0.f,
            0,
            armnn::DataLayout::NCHW);
}

LayerTestResult<float, 4> DepthwiseConvolution2dAsymmetricTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    return DepthwiseConvolution2dAsymmetricTestCommon<armnn::DataType::Float32, armnn::DataType::Float32>(
        workloadFactory, memoryManager, 0.0f, 0, biasEnabled, layout);
}

LayerTestResult<uint8_t, 4> DepthwiseConvolution2dUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    return DepthwiseConvolution2dTestImpl<armnn::DataType::QuantisedAsymm8, armnn::DataType::Signed32>(
        workloadFactory, memoryManager, 0.5f, 50, biasEnabled, layout);
}

LayerTestResult<uint8_t, 4> DepthwiseConvolution2dDepthMul1Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool biasEnabled,
    const armnn::DataLayout layout)
{
    return DepthwiseConvolution2dDepthMul1TestImpl<armnn::DataType::QuantisedAsymm8, armnn::DataType::Signed32>(
        workloadFactory, memoryManager, 0.5f, 50, biasEnabled, layout);
}

LayerTestResult<float, 4> SimpleDepthwiseConvolution2d3x3Dilation3x3NhwcTest(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return SimpleDepthwiseConvolution2d3x3Dilation3x3NhwcTestCommon<armnn::DataType::Float32, armnn::DataType::Float32>(
            workloadFactory,
            memoryManager,
            0.f,
            0,
            false);
}

LayerTestResult<int16_t, 4> DepthwiseConvolution2dInt16Test(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
        bool biasEnabled,
        const armnn::DataLayout layout)
{
    return DepthwiseConvolution2dTestImpl<armnn::DataType::QuantisedSymm16, armnn::DataType::Signed32>(
        workloadFactory, memoryManager, 0.5f, 50, biasEnabled, layout);
}

LayerTestResult<int16_t, 4> DepthwiseConvolution2dDepthMul1Int16Test(
                armnn::IWorkloadFactory& workloadFactory,
                const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
                bool biasEnabled,
                const armnn::DataLayout layout)
{
    return DepthwiseConvolution2dDepthMul1TestImpl<armnn::DataType::QuantisedSymm16, armnn::DataType::Signed32>(
        workloadFactory, memoryManager, 0.5f, 50, biasEnabled, layout);
}

LayerTestResult<float, 4> CompareDepthwiseConvolution2dFloatTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    armnn::IWorkloadFactory& refWorkloadFactory,
    const armnn::DataLayout layout)
{
    return CompareDepthwiseConvolution2dTestImpl<armnn::DataType::Float32>(
        workloadFactory, memoryManager, refWorkloadFactory, layout);
}

LayerTestResult<uint8_t, 4> CompareDepthwiseConvolution2dUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    armnn::IWorkloadFactory& refWorkloadFactory,
    const armnn::DataLayout layout)
{
    return CompareDepthwiseConvolution2dTestImpl<armnn::DataType::QuantisedAsymm8>(
        workloadFactory, memoryManager, refWorkloadFactory, layout);
}
