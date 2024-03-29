//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "ConcatTestImpl.hpp"

#include <Permute.hpp>
#include <ResolveType.hpp>

#include <armnn/ArmNN.hpp>

#include <backendsCommon/test/TensorCopyUtils.hpp>
#include <backendsCommon/test/WorkloadTestUtils.hpp>

#include <test/TensorHelpers.hpp>

//
// Helper functions and templates
//

armnn::OriginsDescriptor CreateDescriptorForConcat(
    const std::vector<armnn::TensorInfo> & inputTensorInfos,
    unsigned int concatDim)
{
    std::vector<armnn::TensorShape> shapes;
    shapes.reserve(inputTensorInfos.size());
    for (const armnn::TensorInfo& it: inputTensorInfos)
    {
        shapes.push_back(it.GetShape());
    }

    return armnn::CreateDescriptorForConcatenation(shapes.begin(), shapes.end(), concatDim);
}

//
// Concat is only supported for N and C dimensions for NCHW and the inner most dimension
// In case of <4 dimensions we need to make sure that the concat dimensions are at least
// the 3rd slowest iterating one or the inner most dimension.
//

bool NeedPermuteForConcat(
    const std::vector<armnn::TensorInfo> & inputTensorInfos,
    unsigned int concatDim)
{
    // See note above. Additionally we expect the input shapes to have the
    // same number of dimensions.
    unsigned int nDimensions = 0;

    // Determine the number of dimensions as well as sanity check them
    // agains test implementation issues.
    for (auto && tensorInfo : inputTensorInfos)
    {
        if (!nDimensions)
        {
            nDimensions = tensorInfo.GetShape().GetNumDimensions();
        }
        else
        {
            BOOST_ASSERT_MSG(nDimensions == tensorInfo.GetShape().GetNumDimensions(),
                "Input shapes must have the same number of dimensions");
        }
    }

    return (nDimensions < 3 || (nDimensions == 3 && (nDimensions-concatDim) < 3 && (nDimensions-concatDim) != 1));
}

armnn::TensorShape ExpandTensorShapeTo3dForPermute(const armnn::TensorShape & inputShape)
{
    unsigned int numDims = inputShape.GetNumDimensions();
    if (numDims >= 3)
    {
        // Nothing to do if the inputShape has at least 3 dimensions.
        return inputShape;
    }

    std::vector<unsigned int> newDims(size_t(3), 1u);
    unsigned int expandedBy = 3 - numDims;
    for (unsigned int i=0; i<numDims; ++i)
    {
        newDims[expandedBy+i] = inputShape[i];
    }
    return armnn::TensorShape(3u, &newDims[0]);
}

void Generate3dPermuteVectorForConcat(
    unsigned int numDimensions,
    unsigned int & concatDim,
    std::pair<armnn::PermutationVector, armnn::PermutationVector> & permutations)
{
    BOOST_ASSERT_MSG(numDimensions <= 3,
       "Only dimensions 1,2 and 3 are supported by this helper");
    unsigned int expandedBy = 3 - numDimensions;
    unsigned int expandedConcatAxis = concatDim + expandedBy;

    if (expandedConcatAxis == 2)
    {
        concatDim = 0;
        armnn::PermutationVector forwardPermutation({1, 2, 0});
        armnn::PermutationVector reversePermutation({2, 0, 1});
        permutations = std::make_pair(forwardPermutation, reversePermutation);
    }
    else if (expandedConcatAxis == 1)
    {
        concatDim = 0;
        armnn::PermutationVector forwardPermutation({2, 0, 1});
        armnn::PermutationVector reversePermutation({1, 2, 0});
        permutations = std::make_pair(forwardPermutation, reversePermutation);
    }
    else
    {
        BOOST_ASSERT(expandedConcatAxis == 0);
        concatDim = 0;
    }
}

template<typename T> void PermuteTensorData(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    const armnn::PermutationVector& mappings,
    armnn::TensorInfo & inputTensorInfo,
    const T * inputData,
    std::vector<T>& outputData)
{
    BOOST_ASSERT_MSG(inputData != nullptr, "inputData must not be null");
    if (inputData == nullptr)
    {
        // Nullptr is an error in the test. By returning without doing the concatenation
        // I expect the caller to fail the test. It still makes sense to report this as
        // an assert for Debug builds.
        return;
    }

    armnn::TensorInfo outputTensorInfo = armnnUtils::Permuted(inputTensorInfo, mappings);

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::PermuteQueueDescriptor queueDescriptor;
    queueDescriptor.m_Parameters = armnn::PermuteDescriptor{mappings};
    armnn::WorkloadInfo workloadInfo;
    AddInputToWorkload(queueDescriptor, workloadInfo, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(queueDescriptor, workloadInfo, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreatePermute(queueDescriptor, workloadInfo);

    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), inputData);

    workload->PostAllocationConfigure();
    workload->Execute();

    outputData.resize(outputTensorInfo.GetNumElements());
    CopyDataFromITensorHandle(&outputData[0], outputHandle.get());
    inputTensorInfo = outputTensorInfo;
}

//
// Permute the input tensors so we can do a supported concatenation.
// Also treat lower than 3d tensors as 3d by adding dummy 1 dimensions
// at the front. Finally this function tells what the output shape
// of the permuted concatenated tensor is going to be.
//
template<typename T> void PermuteInputsForConcat(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    std::vector<armnn::TensorInfo> & inputTensorInfos,
    std::vector<T *> & inputData,
    std::vector<std::vector<T>> & inputDataStorage,
    armnn::PermutationVector & permuteVector,
    unsigned int & concatDim,
    armnn::TensorInfo & outputTensorInfo)
{
    BOOST_ASSERT_MSG(inputTensorInfos.size() > 1,
        "Expecting more than one tensor to be concatenated here");

    unsigned int numDims = 0;
    unsigned int nthInput = 0;
    const armnn::PermutationVector identity({0, 1, 2});

    std::pair<armnn::PermutationVector, armnn::PermutationVector> permutations =
        std::make_pair(identity, identity);

    inputDataStorage.resize(inputData.size());

    for (auto && tensorInfo : inputTensorInfos)
    {
        if (numDims == 0)
        {
            numDims = tensorInfo.GetShape().GetNumDimensions();
            Generate3dPermuteVectorForConcat(numDims, concatDim, permutations);

            // Store the reverese permutation.
            permuteVector = permutations.second;
            BOOST_ASSERT_MSG(!permuteVector.IsEqual(identity),
                "Test logic error, we don't need permutation, so we shouldn't arrive here");
        }
        else
        {
            BOOST_ASSERT_MSG(numDims == tensorInfo.GetShape().GetNumDimensions(),
                "All inputs must have the same number of dimensions");
        }

        armnn::TensorInfo newTensorInfo = tensorInfo;
        newTensorInfo.SetShape(ExpandTensorShapeTo3dForPermute(tensorInfo.GetShape()));

        PermuteTensorData<T>(workloadFactory,
                             memoryManager,
                             permutations.first,
                             newTensorInfo,
                             inputData[nthInput],
                             inputDataStorage[nthInput]);

        inputData[nthInput] = inputDataStorage[nthInput].data();
        inputTensorInfos[nthInput] = newTensorInfo;

        ++nthInput;
    }

    outputTensorInfo.SetShape(
        armnnUtils::Permuted(
            ExpandTensorShapeTo3dForPermute(outputTensorInfo.GetShape()),
            permutations.first));
}

//
// This is the pair of PermuteInputsForConcat(...) which permutes back
// the output of the concatenation so we can check it against an expected
// output.
//
template <typename T> void PermuteOutputForConcat(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    const armnn::TensorInfo & tensorInfo,
    const armnn::PermutationVector & permuteVector,
    std::unique_ptr<armnn::ITensorHandle> && inputDataHandle,
    T * data)
{
    BOOST_ASSERT_MSG(data != nullptr, "data must not be null");
    if (data == nullptr)
    {
        // Nullptr is an error in the test. By returning without doing the permutation
        // I expect the caller to fail the test. It still makes sense to report this as
        // an assert for Debug builds.
        return;
    }

    armnn::TensorInfo resultTensorInfo = tensorInfo;
    std::vector<T> inputData(tensorInfo.GetNumElements());
    std::vector<T> outputData;

    CopyDataFromITensorHandle(&inputData[0], inputDataHandle.get());

    PermuteTensorData<T>(workloadFactory,
                         memoryManager,
                         permuteVector,
                         resultTensorInfo,
                         &inputData[0],
                         outputData);

    ::memcpy(data, &outputData[0], sizeof(T)*outputData.size());
}

template<typename T> void Concatenate(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    std::initializer_list<const armnn::TensorInfo> inputTensorInfosOrig,
    std::initializer_list<T *> inputsOrig,
    const armnn::TensorInfo& outputTensorInfoOrig,
    T * output,
    unsigned int concatDim,
    bool useSubtensor)
{
    BOOST_ASSERT_MSG(output != nullptr, "output must not be null");
    if (output == nullptr)
    {
        // Nullptr is an error in the test. By returning without doing the permutation
        // I expect the caller to fail the test. It still makes sense to report this as
        // an assert for Debug builds.
        return;
    }

    // Saves a copy of the parameters which we might need to change.
    std::vector<armnn::TensorInfo> inputTensorInfos(inputTensorInfosOrig.begin(), inputTensorInfosOrig.end());
    std::vector<T *> inputs            = inputsOrig;
    armnn::TensorInfo outputTensorInfo = outputTensorInfoOrig;

    armnn::PermutationVector permuteVector{0, 1, 2};

    // Holds and automatically releases memory for the reshaped input data.
    std::vector<std::vector<T>> tmpInputDataStorage;

    const size_t inputCount = inputTensorInfos.size();

    bool needPermuteForConcat = NeedPermuteForConcat(inputTensorInfos, concatDim);

    if (needPermuteForConcat)
    {
        //
        // We need to permute the inputs, because concatenation along
        // the requested axis is not supported.
        //
        PermuteInputsForConcat<T>(workloadFactory,
                                  memoryManager,
                                  inputTensorInfos,
                                  inputs,
                                  tmpInputDataStorage,
                                  permuteVector,
                                  concatDim,
                                  outputTensorInfo);
    }

    armnn::WorkloadInfo workloadInfo;

    std::vector<std::unique_ptr<armnn::ITensorHandle>> inputHandles;
    inputHandles.reserve(inputCount);

    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    armnn::ConcatQueueDescriptor queueDescriptor;
    armnn::OriginsDescriptor viewsDescriptor = CreateDescriptorForConcat(inputTensorInfos, concatDim);
    queueDescriptor.m_Parameters = viewsDescriptor;

    if (useSubtensor)
    {
        queueDescriptor.m_ViewOrigins.reserve(viewsDescriptor.GetNumViews());
        for (unsigned int i = 0; i < viewsDescriptor.GetNumViews(); ++i)
        {
            queueDescriptor.m_ViewOrigins.emplace_back(std::vector<unsigned int>(viewsDescriptor.GetViewOrigin(i),
                viewsDescriptor.GetViewOrigin(i) + viewsDescriptor.GetNumDimensions()));
        }

        outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

        const bool subTensorsSupported = workloadFactory.SupportsSubTensors();
        for (unsigned int i = 0; i < inputCount; ++i)
        {
            const armnn::TensorInfo& inputTensorInfo = inputTensorInfos[i];
            std::unique_ptr<armnn::ITensorHandle> inputHandle =
                subTensorsSupported ?
                    workloadFactory.CreateSubTensorHandle(*outputHandle,
                                                          inputTensorInfo.GetShape(),
                                                          queueDescriptor.m_ViewOrigins[i].m_Origin.data()) :
                    workloadFactory.CreateTensorHandle(inputTensorInfo);

            inputHandles.emplace_back(std::move(inputHandle));
        }

    }
    else
    {
        for (unsigned int i = 0; i < inputCount; ++i)
        {
            std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfos[i]);
            inputHandles.emplace_back(std::move(inputHandle));
        }
    }

    for (unsigned int i = 0; i < inputCount; ++i)
    {
        AddInputToWorkload(queueDescriptor, workloadInfo, inputTensorInfos[i], inputHandles[i].get());
    }

    AddOutputToWorkload(queueDescriptor, workloadInfo, outputTensorInfo, outputHandle.get());

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateConcat(queueDescriptor, workloadInfo);

    for (auto& inputHandle : inputHandles)
    {
        inputHandle->Allocate();
    }

    outputHandle->Allocate();

    unsigned int nextInputId = 0;
    for (auto& inputHandle : inputHandles)
    {
        CopyDataToITensorHandle(inputHandle.get(), inputs[nextInputId]);
        ++nextInputId;
    }

    workload->PostAllocationConfigure();
    workload->Execute();

    if (needPermuteForConcat)
    {
        PermuteOutputForConcat<T>(workloadFactory,
                                  memoryManager,
                                  outputTensorInfo,
                                  permuteVector,
                                  std::move(outputHandle),
                                  output);
    }
    else
    {
        CopyDataFromITensorHandle(output, outputHandle.get());
    }
}

//
// Implementation templates
//

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 1> Concat1dTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo inputTensorInfo({ 3 }, ArmnnType, qScale, qOffset);

    auto input0 = MakeTensor<T, 1>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, { 1.0f, 2.0f, 3.0f }));
    auto input1 = MakeTensor<T, 1>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, { 4.0f, 5.0f, 6.0f }));
    auto input2 = MakeTensor<T, 1>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, { 7.0f, 8.0f, 9.0f }));

    armnn::TensorInfo outputTensorInfo({ 9 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 1> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate<T>(workloadFactory, memoryManager,
                   { inputTensorInfo, inputTensorInfo, inputTensorInfo },
                   { input0.data(), input1.data(), input2.data() },
                   outputTensorInfo,
                   output.data(),
                   0,
                   true);

    result.output = MakeTensor<T, 1>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 1>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 2> Concat2dTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    const armnn::TensorInfo& outputTensorInfo,
    unsigned int dimension,
    const float qScale,
    const int32_t qOffset)
{
    armnn::TensorInfo inputTensorInfo({ 2, 3 }, ArmnnType, qScale, qOffset);

    auto input0 = MakeTensor<T, 2>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f,
    }));

    auto input1 = MakeTensor<T, 2>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        4.0f, 5.0f, 6.0f,

        // Batch 1
        13.0f, 14.0f, 15.0f,
    }));

    auto input2 = MakeTensor<T, 2>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        7.0f, 8.0f, 9.0f,

        // Batch 1
        16.0f, 17.0f, 18.0f,
    }));

    LayerTestResult<T, 2> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate<T>(workloadFactory, memoryManager,
                   { inputTensorInfo, inputTensorInfo, inputTensorInfo },
                   { input0.data(), input1.data(), input2.data() },
                   outputTensorInfo,
                   output.data(),
                   dimension,
                   true);

    result.output = MakeTensor<T, 2>(outputTensorInfo, output);
    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 2> Concat2dDim0TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo outputTensorInfo({ 6, 3 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 2> result = Concat2dTestImpl<ArmnnType>(
        workloadFactory, memoryManager, outputTensorInfo, 0, qScale, qOffset);

    result.outputExpected = MakeTensor<T, 2>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f,

        // Batch 2
        4.0f, 5.0f, 6.0f,

        // Batch 3
        13.0f, 14.0f, 15.0f,

        // Batch 4
        7.0f, 8.0f, 9.0f,

        // Batch 5
        16.0f, 17.0f, 18.0f,
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 2> Concat2dDim1TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo outputTensorInfo({ 2, 9 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 2> result = Concat2dTestImpl<ArmnnType>(
        workloadFactory, memoryManager, outputTensorInfo, 1, qScale, qOffset);

    result.outputExpected = MakeTensor<T, 2>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 2> Concat2dDim0DiffInputDimsTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo input0TensorInfo({ 2, 3 }, ArmnnType, qScale, qOffset);
    auto input0 = MakeTensor<T, 2>(input0TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f,
    }));

    armnn::TensorInfo input1TensorInfo({ 3, 3 }, ArmnnType, qScale, qOffset);
    auto input1 = MakeTensor<T, 2>(input1TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        4.0f, 5.0f, 6.0f,

        // Batch 1
        13.0f, 14.0f, 15.0f,

        // Batch 0
        7.0f, 8.0f, 9.0f,
    }));

    armnn::TensorInfo input2TensorInfo({ 1, 3 }, ArmnnType, qScale, qOffset);
    auto input2 = MakeTensor<T, 2>(input2TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 1
        16.0f, 17.0f, 18.0f,
    }));

    armnn::TensorInfo outputTensorInfo({ 6, 3 }, ArmnnType, qScale, qOffset);
    LayerTestResult<T, 2> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate<T>(workloadFactory, memoryManager,
                   { input0TensorInfo, input1TensorInfo, input2TensorInfo },
                   { input0.data(), input1.data(), input2.data() },
                   outputTensorInfo,
                   output.data(),
                   0,
                   true);

    result.output = MakeTensor<T, 2>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 2>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f,

        // Batch 2
        4.0f, 5.0f, 6.0f,

        // Batch 3
        13.0f, 14.0f, 15.0f,

        // Batch 4
        7.0f, 8.0f, 9.0f,

        // Batch 5
        16.0f, 17.0f, 18.0f,
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 2> Concat2dDim1DiffInputDimsTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo input0TensorInfo({ 2, 3 }, ArmnnType, qScale, qOffset);
    auto input0 = MakeTensor<T, 2>(input0TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f,
    }));

    armnn::TensorInfo input1TensorInfo({ 2, 5 }, ArmnnType, qScale, qOffset);
    auto input1 = MakeTensor<T, 2>(input1TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        4.0f, 5.0f, 6.0f, 7.0f, 8.0f,

        // Batch 1
        13.0f, 14.0f, 15.0f, 16.0f, 17.0f,
    }));

    armnn::TensorInfo input2TensorInfo({ 2, 1 }, ArmnnType, qScale, qOffset);
    auto input2 = MakeTensor<T, 2>(input2TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        9.0f,

        // Batch 1
        18.0f
    }));

    armnn::TensorInfo outputTensorInfo({ 2, 9 }, ArmnnType, qScale, qOffset);
    LayerTestResult<T, 2> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate<T>(workloadFactory, memoryManager,
                   { input0TensorInfo, input1TensorInfo, input2TensorInfo },
                   { input0.data(), input1.data(), input2.data() },
                   outputTensorInfo,
                   output.data(),
                   1,
                   true);

    result.output = MakeTensor<T, 2>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 2>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f,

        // Batch 1
        10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f,
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 3> Concat3dTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    const armnn::TensorInfo& outputTensorInfo,
    unsigned int dimension,
    bool useSubtensor,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo inputTensorInfo({ 2, 3, 2 }, ArmnnType, qScale, qOffset);

    auto input0 = MakeTensor<T, 3>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f
    }));

    auto input1 = MakeTensor<T, 3>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        7.0f, 8.0f,

        // Batch 0, Channel 1
        9.0f, 10.0f,

        // Batch 0, Channel 2
        11.0f, 12.0f,

        // Batch 1, Channel 0
        25.0f, 26.0f,

        // Batch 1, Channel 1
        27.0f, 28.0f,

        // Batch 1, Channel 2
        29.0f, 30.0f
    }));

    auto input2 = MakeTensor<T, 3>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        13.0f, 14.0f,

        // Batch 0, Channel 1
        15.0f, 16.0f,

        // Batch 0, Channel 2
        17.0f, 18.0f,

        // Batch 1, Channel 0
        31.0f, 32.0f,

        // Batch 1, Channel 1
        33.0f, 34.0f,

        // Batch 1, Channel 2
        35.0f, 36.0f
    }));

    LayerTestResult<T, 3> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate<T>(workloadFactory, memoryManager,
                   { inputTensorInfo, inputTensorInfo, inputTensorInfo },
                   { input0.data(), input1.data(), input2.data() },
                   outputTensorInfo,
                   output.data(),
                   dimension,
                   useSubtensor);

    result.output = MakeTensor<T, 3>(outputTensorInfo, output);
    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 3> Concat3dDim0TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo outputTensorInfo({ 6, 3, 2 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 3> result = Concat3dTestImpl<ArmnnType>(
        workloadFactory, memoryManager, outputTensorInfo, 0, true, qScale, qOffset);

    result.outputExpected = MakeTensor<T, 3>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f,

        // Batch 2, Channel 0
        7.0f, 8.0f,

        // Batch 2, Channel 1
        9.0f, 10.0f,

        // Batch 2, Channel 2
        11.0f, 12.0f,

        // Batch 3, Channel 0
        25.0f, 26.0f,

        // Batch 3, Channel 1
        27.0f, 28.0f,

        // Batch 3, Channel 2
        29.0f, 30.0f,

        // Batch 4, Channel 0
        13.0f, 14.0f,

        // Batch 4, Channel 1
        15.0f, 16.0f,

        // Batch 4, Channel 2
        17.0f, 18.0f,

        // Batch 5, Channel 0
        31.0f, 32.0f,

        // Batch 5, Channel 1
        33.0f, 34.0f,

        // Batch 5, Channel 2
        35.0f, 36.0f
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 3> Concat3dDim1TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo outputTensorInfo({ 2, 9, 2 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 3> result = Concat3dTestImpl<ArmnnType>(
        workloadFactory, memoryManager, outputTensorInfo, 1, true, qScale, qOffset);

    result.outputExpected = MakeTensor<T, 3>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 0, Channel 3
        7.0f, 8.0f,

        // Batch 0, Channel 4
        9.0f, 10.0f,

        // Batch 0, Channel 5
        11.0f, 12.0f,

        // Batch 0, Channel 6
        13.0f, 14.0f,

        // Batch 0, Channel 7
        15.0f, 16.0f,

        // Batch 0, Channel 8
        17.0f, 18.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f,

        // Batch 1, Channel 3
        25.0f, 26.0f,

        // Batch 1, Channel 4
        27.0f, 28.0f,

        // Batch 1, Channel 5
        29.0f, 30.0f,

        // Batch 1, Channel 6
        31.0f, 32.0f,

        // Batch 1, Channel 7
        33.0f, 34.0f,

        // Batch 1, Channel 8
        35.0f, 36.0f
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 3> Concat3dDim2TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool useSubtensor,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo outputTensorInfo({ 2, 3, 6 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 3> result = Concat3dTestImpl<ArmnnType>(
        workloadFactory, memoryManager, outputTensorInfo, 2, useSubtensor, qScale, qOffset);

    result.outputExpected = MakeTensor<T, 3>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f, 7.0f, 8.0f, 13.0f, 14.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f, 9.0f, 10.0f, 15.0f, 16.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f, 11.0f, 12.0f, 17.0f, 18.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f, 25.0f, 26.0f, 31.0f, 32.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f, 27.0f, 28.0f, 33.0f, 34.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f, 29.0f, 30.0f, 35.0f, 36.0f,
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 3> Concat3dDim0DiffInputDimsTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo input0TensorInfo({ 2, 3, 2 }, ArmnnType);
    auto input0 = MakeTensor<T, 3>(input0TensorInfo, QuantizedVector<T>(qScale, qOffset, {
            // Batch 0, Channel 0
            1.0f, 2.0f,

            // Batch 0, Channel 1
            3.0f, 4.0f,

            // Batch 0, Channel 2
            5.0f, 6.0f,

            // Batch 1, Channel 0
            19.0f, 20.0f,

            // Batch 1, Channel 1
            21.0f, 22.0f,

            // Batch 1, Channel 2
            23.0f, 24.0f
    }));

    armnn::TensorInfo input1TensorInfo({ 1, 3, 2 }, ArmnnType);
    auto input1 = MakeTensor<T, 3>(input1TensorInfo, QuantizedVector<T>(qScale, qOffset, {
            // Batch 0, Channel 0
            7.0f, 8.0f,

            // Batch 0, Channel 1
            9.0f, 10.0f,

            // Batch 0, Channel 2
            11.0f, 12.0f,
    }));

    armnn::TensorInfo input2TensorInfo({ 3, 3, 2 }, ArmnnType);
    auto input2 = MakeTensor<T, 3>(input2TensorInfo, QuantizedVector<T>(qScale, qOffset, {
            // Batch 0, Channel 0
            25.0f, 26.0f,

            // Batch 0, Channel 1
            27.0f, 28.0f,

            // Batch 0, Channel 2
            29.0f, 30.0f,

            // Batch 1, Channel 0
            13.0f, 14.0f,

            // Batch 1, Channel 1
            15.0f, 16.0f,

            // Batch 1, Channel 2
            17.0f, 18.0f,

            // Batch 2, Channel 0
            31.0f, 32.0f,

            // Batch 2, Channel 1
            33.0f, 34.0f,

            // Batch 2, Channel 2
            35.0f, 36.0f
    }));

    armnn::TensorInfo outputTensorInfo({ 6, 3, 2 }, ArmnnType);
    LayerTestResult<T, 3> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate<T>(workloadFactory, memoryManager,
                   { input0TensorInfo, input1TensorInfo, input2TensorInfo },
                   { input0.data(), input1.data(), input2.data() },
                   outputTensorInfo,
                   output.data(),
                   0,
                   true);

    result.output = MakeTensor<T, 3>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 3>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f,

        // Batch 2, Channel 0
        7.0f, 8.0f,

        // Batch 2, Channel 1
        9.0f, 10.0f,

        // Batch 2, Channel 2
        11.0f, 12.0f,

        // Batch 3, Channel 0
        25.0f, 26.0f,

        // Batch 3, Channel 1
        27.0f, 28.0f,

        // Batch 3, Channel 2
        29.0f, 30.0f,

        // Batch 4, Channel 0
        13.0f, 14.0f,

        // Batch 4, Channel 1
        15.0f, 16.0f,

        // Batch 4, Channel 2
        17.0f, 18.0f,

        // Batch 5, Channel 0
        31.0f, 32.0f,

        // Batch 5, Channel 1
        33.0f, 34.0f,

        // Batch 5, Channel 2
        35.0f, 36.0f
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 3> Concat3dDim1DiffInputDimsTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo input0TensorInfo({ 2, 3, 2 }, ArmnnType, qScale, qOffset);
    auto input0 = MakeTensor<T, 3>(input0TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f
    }));

    armnn::TensorInfo input1TensorInfo({ 2, 4, 2 }, ArmnnType, qScale, qOffset);
    auto input1 = MakeTensor<T, 3>(input1TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        7.0f, 8.0f,

        // Batch 0, Channel 1
        9.0f, 10.0f,

        // Batch 0, Channel 2
        11.0f, 12.0f,

        // Batch 0, Channel 3
        25.0f, 26.0f,

        // Batch 1, Channel 0
        27.0f, 28.0f,

        // Batch 1, Channel 1
        29.0f, 30.0f,

        // Batch 1, Channel 2
        13.0f, 14.0f,

        // Batch 1, Channel 3
        15.0f, 16.0f,
    }));

    armnn::TensorInfo input2TensorInfo({ 2, 1, 2 }, ArmnnType, qScale, qOffset);
    auto input2 = MakeTensor<T, 3>(input2TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        17.0f, 18.0f,

        // Batch 1, Channel 0
        31.0f, 32.0f,
    }));

    armnn::TensorInfo outputTensorInfo({ 2, 8, 2 }, ArmnnType, qScale, qOffset);
    LayerTestResult<T, 3> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate<T>(workloadFactory, memoryManager,
                   { input0TensorInfo, input1TensorInfo, input2TensorInfo },
                   { input0.data(), input1.data(), input2.data() },
                   outputTensorInfo,
                   output.data(),
                   1,
                   true);

    result.output = MakeTensor<T, 3>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 3>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 0, Channel 3
        7.0f, 8.0f,

        // Batch 0, Channel 4
        9.0f, 10.0f,

        // Batch 0, Channel 5
        11.0f, 12.0f,

        // Batch 0, Channel 6
        25.0f, 26.0f,

        // Batch 0, Channel 7
        17.0f, 18.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f,

        // Batch 1, Channel 3
        27.0f, 28.0f,

        // Batch 1, Channel 4
        29.0f, 30.0f,

        // Batch 1, Channel 5
        13.0f, 14.0f,

        // Batch 1, Channel 6
        15.0f, 16.0f,

        // Batch 1, Channel 7
        31.0f, 32.0f,
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 3> Concat3dDim2DiffInputDimsTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool useSubtensor,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo input0TensorInfo({ 2, 3, 2 }, ArmnnType, qScale, qOffset);
    auto input0 = MakeTensor<T, 3>(input0TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f
    }));

    armnn::TensorInfo input1TensorInfo({ 2, 3, 1 }, ArmnnType, qScale, qOffset);
    auto input1 = MakeTensor<T, 3>(input1TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        7.0f,

        // Batch 0, Channel 1
        9.0f,

        // Batch 0, Channel 2
        11.0f,

        // Batch 1, Channel 0
        25.0f,

        // Batch 1, Channel 1
        27.0f,

        // Batch 1, Channel 2
        29.0f
    }));

    armnn::TensorInfo input2TensorInfo({ 2, 3, 3 }, ArmnnType, qScale, qOffset);
    auto input2 = MakeTensor<T, 3>(input2TensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        13.0f, 14.0f, 50.0f,

        // Batch 0, Channel 1
        15.0f, 16.0f, 51.0f,

        // Batch 0, Channel 2
        17.0f, 18.0f, 52.0f,

        // Batch 1, Channel 0
        31.0f, 32.0f, 53.0f,

        // Batch 1, Channel 1
        33.0f, 34.0f, 54.0f,

        // Batch 1, Channel 2
        35.0f, 36.0f, 55.0f,
    }));

    armnn::TensorInfo outputTensorInfo({ 2, 3, 6 }, ArmnnType, qScale, qOffset);
    LayerTestResult<T, 3> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate<T>(workloadFactory, memoryManager,
                   { input0TensorInfo, input1TensorInfo, input2TensorInfo },
                   { input0.data(), input1.data(), input2.data() },
                   outputTensorInfo,
                   output.data(),
                   2,
                   useSubtensor);

    result.output = MakeTensor<T, 3>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 3>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        // Batch 0, Channel 0
        1.0f, 2.0f, 7.0f, 13.0f, 14.0f, 50.0f,

        // Batch 0, Channel 1
        3.0f, 4.0f, 9.0f, 15.0f, 16.0f, 51.0f,

        // Batch 0, Channel 2
        5.0f, 6.0f, 11.0f, 17.0f, 18.0f, 52.0f,

        // Batch 1, Channel 0
        19.0f, 20.0f, 25.0f, 31.0f, 32.0f, 53.0f,

        // Batch 1, Channel 1
        21.0f, 22.0f, 27.0f, 33.0f, 34.0f, 54.0f,

        // Batch 1, Channel 2
        23.0f, 24.0f, 29.0f, 35.0f, 36.0f, 55.0f,
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> Concat4dTestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    const armnn::TensorInfo& outputTensorInfo,
    unsigned int dimension,
    bool useSubtensor,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo inputTensorInfo({ 1, 3, 2, 2 }, ArmnnType, qScale, qOffset);

    auto input0 = MakeTensor<T, 4>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f,
        7.0f, 8.0f,
        9.0f, 10.0f,
        11.0f, 12.0f
    }));

    auto input1 = MakeTensor<T, 4>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        11.0f, 12.0f,
        13.0f, 14.0f,
        15.0f, 16.0f,
        17.0f, 18.0f,
        19.0f, 20.0f,
        21.0f, 22.0f
    }));

    auto input2 = MakeTensor<T, 4>(inputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        21.0f, 22.0f,
        23.0f, 24.0f,
        25.0f, 26.0f,
        27.0f, 28.0f,
        29.0f, 30.0f,
        31.0f, 32.0f
    }));

    LayerTestResult<T, 4> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());

    Concatenate<T>(workloadFactory,
                   memoryManager,
                   {inputTensorInfo, inputTensorInfo, inputTensorInfo},
                   {input0.data(), input1.data(), input2.data()},
                   outputTensorInfo,
                   output.data(),
                   dimension,
                   useSubtensor);

    result.output = MakeTensor<T, 4>(outputTensorInfo, output);
    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> Concat4dDim0TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo outputTensorInfo({ 3, 3, 2, 2 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 4> result = Concat4dTestImpl<ArmnnType>(
        workloadFactory, memoryManager, outputTensorInfo, 0, true, qScale, qOffset);

    result.outputExpected = MakeTensor<T, 4>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f,
        7.0f, 8.0f,
        9.0f, 10.0f,
        11.0f, 12.0f,

        11.0f, 12.0f,
        13.0f, 14.0f,
        15.0f, 16.0f,
        17.0f, 18.0f,
        19.0f, 20.0f,
        21.0f, 22.0f,

        21.0f, 22.0f,
        23.0f, 24.0f,
        25.0f, 26.0f,
        27.0f, 28.0f,
        29.0f, 30.0f,
        31.0f, 32.0f
    }));
    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> Concat4dDim1TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo outputTensorInfo({ 1, 9, 2, 2 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 4> result = Concat4dTestImpl<ArmnnType>(
        workloadFactory, memoryManager, outputTensorInfo, 1, true, qScale, qOffset);

    result.outputExpected = MakeTensor<T, 4>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f,
        7.0f, 8.0f,
        9.0f, 10.0f,
        11.0f, 12.0f,

        11.0f, 12.0f,
        13.0f, 14.0f,
        15.0f, 16.0f,
        17.0f, 18.0f,
        19.0f, 20.0f,
        21.0f, 22.0f,

        21.0f, 22.0f,
        23.0f, 24.0f,
        25.0f, 26.0f,
        27.0f, 28.0f,
        29.0f, 30.0f,
        31.0f, 32.0f
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> Concat4dDim2TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    armnn::TensorInfo outputTensorInfo({ 1, 3, 6, 2 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 4> result = Concat4dTestImpl<ArmnnType>(
        workloadFactory, memoryManager, outputTensorInfo, 2, true, qScale, qOffset);

    result.outputExpected = MakeTensor<T, 4>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f,
        3.0f, 4.0f,
        11.0f, 12.0f,
        13.0f, 14.0f,
        21.0f, 22.0f,
        23.0f, 24.0f,

        5.0f, 6.0f,
        7.0f, 8.0f,
        15.0f, 16.0f,
        17.0f, 18.0f,
        25.0f, 26.0f,
        27.0f, 28.0f,

        9.0f, 10.0f,
        11.0f, 12.0f,
        19.0f, 20.0f,
        21.0f, 22.0f,
        29.0f, 30.0f,
        31.0f, 32.0f
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> Concat4dDim3TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset,
    bool useSubtensor)
{
    armnn::TensorInfo outputTensorInfo({ 1, 3, 2, 6 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 4> result = Concat4dTestImpl<ArmnnType>(
        workloadFactory, memoryManager, outputTensorInfo, 3, useSubtensor, qScale, qOffset);

    result.outputExpected = MakeTensor<T, 4>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f,
        11.0f, 12.0f,
        21.0f, 22.0f,
        3.0f, 4.0f,
        13.0f, 14.0f,
        23.0f, 24.0f,

        5.0f, 6.0f,
        15.0f, 16.0f,
        25.0f, 26.0f,
        7.0f, 8.0f,
        17.0f, 18.0f,
        27.0f, 28.0f,

        9.0f, 10.0f,
        19.0f, 20.0f,
        29.0f, 30.0f,
        11.0f, 12.0f,
        21.0f, 22.0f,
        31.0f, 32.0f
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> Concat4dDiffShapeDim0TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    unsigned int dimension = 0;
    armnn::TensorInfo inputTensorInfo0({ 1, 3, 2, 2 }, ArmnnType, qScale, qOffset);

    auto input0 = MakeTensor<T, 4>(inputTensorInfo0, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f,
        7.0f, 8.0f,
        9.0f, 10.0f,
        11.0f, 12.0f
    }));

    armnn::TensorInfo inputTensorInfo1({ 2, 3, 2, 2 }, ArmnnType, qScale, qOffset);

    auto input1 = MakeTensor<T, 4>(inputTensorInfo1, QuantizedVector<T>(qScale, qOffset, {
        11.0f, 12.0f,
        13.0f, 14.0f,
        15.0f, 16.0f,
        17.0f, 18.0f,
        19.0f, 20.0f,
        21.0f, 22.0f,

        21.0f, 22.0f,
        23.0f, 24.0f,
        25.0f, 26.0f,
        27.0f, 28.0f,
        29.0f, 30.0f,
        31.0f, 32.0f

    }));

    armnn::TensorInfo outputTensorInfo({ 3, 3, 2, 2 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 4> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate<T>(workloadFactory,
                   memoryManager,
                   {inputTensorInfo0, inputTensorInfo1},
                   {input0.data(), input1.data()},
                   outputTensorInfo,
                   output.data(),
                   dimension,
                   true);

    result.output = MakeTensor<T, 4>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 4>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f,
        7.0f, 8.0f,
        9.0f, 10.0f,
        11.0f, 12.0f,

        11.0f, 12.0f,
        13.0f, 14.0f,
        15.0f, 16.0f,
        17.0f, 18.0f,
        19.0f, 20.0f,
        21.0f, 22.0f,

        21.0f, 22.0f,
        23.0f, 24.0f,
        25.0f, 26.0f,
        27.0f, 28.0f,
        29.0f, 30.0f,
        31.0f, 32.0f
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> Concat4dDiffShapeDim1TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    unsigned int dimension = 1;
    armnn::TensorInfo inputTensorInfo0({ 1, 3, 2, 2 }, ArmnnType, qScale, qOffset);

    auto input0 = MakeTensor<T, 4>(inputTensorInfo0, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f,
        7.0f, 8.0f,
        9.0f, 10.0f,
        11.0f, 12.0f
    }));

    armnn::TensorInfo inputTensorInfo1({ 1, 2, 2, 2 }, ArmnnType, qScale, qOffset);

    auto input1 = MakeTensor<T, 4>(inputTensorInfo1, QuantizedVector<T>(qScale, qOffset, {
        11.0f, 12.0f,
        13.0f, 14.0f,
        15.0f, 16.0f,
        17.0f, 18.0f,

    }));

    armnn::TensorInfo outputTensorInfo({ 1, 5, 2, 2 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 4> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate<T>(workloadFactory,
                   memoryManager,
                   {inputTensorInfo0, inputTensorInfo1},
                   {input0.data(), input1.data()},
                   outputTensorInfo,
                   output.data(),
                   dimension,
                   true);

    result.output = MakeTensor<T, 4>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 4>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f,
        7.0f, 8.0f,
        9.0f, 10.0f,
        11.0f, 12.0f,
        11.0f, 12.0f,
        13.0f, 14.0f,
        15.0f, 16.0f,
        17.0f, 18.0f
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> Concat4dDiffShapeDim2TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset)
{
    unsigned int dimension = 2;
    armnn::TensorInfo inputTensorInfo0({ 1, 3, 2, 2 }, ArmnnType, qScale, qOffset);

    auto input0 = MakeTensor<T, 4>(inputTensorInfo0, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f,
        7.0f, 8.0f,
        9.0f, 10.0f,
        11.0f, 12.0f
    }));

    armnn::TensorInfo inputTensorInfo1({ 1, 3, 3, 2 }, ArmnnType, qScale, qOffset);

    auto input1 = MakeTensor<T, 4>(inputTensorInfo1, QuantizedVector<T>(qScale, qOffset, {
        11.0f, 12.0f,
        13.0f, 14.0f,
        15.0f, 16.0f,
        17.0f, 18.0f,
        19.0f, 20.0f,
        21.0f, 22.0f,
        23.0f, 24.0f,
        25.0f, 26.0f,
        27.0f, 28.0f
    }));

    armnn::TensorInfo outputTensorInfo({ 1, 3, 5, 2 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 4> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate<T>(workloadFactory,
                   memoryManager,
                   {inputTensorInfo0, inputTensorInfo1},
                   {input0.data(), input1.data()},
                   outputTensorInfo,
                   output.data(),
                   dimension,
                   true);

    result.output = MakeTensor<T, 4>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 4>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f,
        3.0f, 4.0f,
        11.0f, 12.0f,
        13.0f, 14.0f,
        15.0f, 16.0f,

        5.0f, 6.0f,
        7.0f, 8.0f,
        17.0f, 18.0f,
        19.0f, 20.0f,
        21.0f, 22.0f,

        9.0f, 10.0f,
        11.0f, 12.0f,
        23.0f, 24.0f,
        25.0f, 26.0f,
        27.0f, 28.0f
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T = armnn::ResolveType<ArmnnType>>
LayerTestResult<T, 4> Concat4dDiffShapeDim3TestImpl(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    float qScale,
    int32_t qOffset,
    bool useSubtensor)
{
    unsigned int dimension = 3;
    armnn::TensorInfo inputTensorInfo0({ 1, 3, 2, 2 }, ArmnnType, qScale, qOffset);

    auto input0 = MakeTensor<T, 4>(inputTensorInfo0, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f,
        7.0f, 8.0f,
        9.0f, 10.0f,
        11.0f, 12.0f
    }));

    armnn::TensorInfo inputTensorInfo1({ 1, 3, 2, 3 }, ArmnnType, qScale, qOffset);

    auto input1 = MakeTensor<T, 4>(inputTensorInfo1, QuantizedVector<T>(qScale, qOffset, {
        11.0f, 12.0f, 13.0f,
        14.0f, 15.0f, 16.0f,

        17.0f, 18.0f, 19.0f,
        20.0f, 21.0f, 22.0f,

        23.0f, 24.0f, 25.0f,
        26.0f, 27.0f, 28.0f
    }));

    armnn::TensorInfo outputTensorInfo({ 1, 3, 2, 5 }, ArmnnType, qScale, qOffset);

    LayerTestResult<T, 4> result(outputTensorInfo);

    std::vector<T> output;
    output.resize(outputTensorInfo.GetNumElements());
    Concatenate<T>(workloadFactory,
                   memoryManager,
                   {inputTensorInfo0, inputTensorInfo1},
                   {input0.data(), input1.data()},
                   outputTensorInfo,
                   output.data(),
                   dimension,
                   useSubtensor);

    result.output = MakeTensor<T, 4>(outputTensorInfo, output);
    result.outputExpected = MakeTensor<T, 4>(outputTensorInfo, QuantizedVector<T>(qScale, qOffset, {
        1.0f, 2.0f, 11.0f, 12.0f, 13.0f,
        3.0f, 4.0f, 14.0f, 15.0f, 16.0f,
        5.0f, 6.0f, 17.0f, 18.0f, 19.0f,
        7.0f, 8.0f, 20.0f, 21.0f, 22.0f,
        9.0f, 10.0f, 23.0f, 24.0f, 25.0f,
        11.0f, 12.0f, 26.0f, 27.0f, 28.0f
    }));

    return result;
}

template<armnn::DataType ArmnnType, typename T>
LayerTestResult<T, 3> ConcatDifferentInputOutputQParamTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool useSubtensor)
{
    // Defines the tensor descriptors.
    armnn::TensorInfo outputTensorInfo({ 3, 6, 3 }, ArmnnType);
    armnn::TensorInfo inputTensorInfo1({ 3, 6, 2 }, ArmnnType);
    armnn::TensorInfo inputTensorInfo2({ 3, 6, 1 }, ArmnnType);

    std::vector<armnn::TensorShape> inputTensorShapes({inputTensorInfo1.GetShape(), inputTensorInfo2.GetShape()});

    // Quantized input1 tensor.
    const float inputScale1 = 0.5f;
    const int32_t inputOffset1 = 5;

    auto input1 = MakeTensor<T, 3>(inputTensorInfo1, std::vector<T>(
    {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9,
        10, 11, 12,
        13, 14, 15,
        16, 17, 18,

        19, 20, 21,
        22, 23, 24,
        25, 26, 27,
        28, 29, 30,
        31, 32, 33,
        34, 35, 36
    }));

    // Quatized input2 tensor.
    const float inputScale2 = 0.2f;
    const int32_t inputOffset2 = 10;

    auto input2 = MakeTensor<T, 3>(inputTensorInfo2, std::vector<T>(
    {
        37, 38, 39,
        40, 41, 42,
        43, 44, 45,
        46, 47, 48,
        49, 50, 51,
        52, 53, 54
    }));

    // Quantized output tensor.
    const float outputScale = 0.1f;
    const int32_t outputOffset = 20;

    LayerTestResult<T, 3> ret(outputTensorInfo);

    ret.outputExpected = MakeTensor<T, 3>(outputTensorInfo, std::vector<T>(
    {
        0,   5,  74,
        10,  15,  76,
        20,  25,  78,
        30,  35,  80,
        40,  45,  82,
        50,  55,  84,

        60,  65,  86,
        70,  75,  88,
        80,  85,  90,
        90,  95,  92,
        100, 105,  94,
        110, 115,  96,

        120, 125,  98,
        130, 135, 100,
        140, 145, 102,
        150, 155, 104,
        160, 165, 106,
        170, 175, 108
    }));

    outputTensorInfo.SetQuantizationScale(outputScale);
    outputTensorInfo.SetQuantizationOffset(outputOffset);
    inputTensorInfo1.SetQuantizationScale(inputScale1);
    inputTensorInfo1.SetQuantizationOffset(inputOffset1);
    inputTensorInfo2.SetQuantizationScale(inputScale2);
    inputTensorInfo2.SetQuantizationOffset(inputOffset2);

    std::vector<unsigned int> wOrigin1 = { 0, 0, 0 }; //Extent of the window is defined by size of input[0].
    armnn::ConcatQueueDescriptor::ViewOrigin window1(wOrigin1);

    std::vector<unsigned int> wOrigin2 = { 0, 0, 2 }; //Extent of the window is defined by size of input[1].
    armnn::ConcatQueueDescriptor::ViewOrigin window2(wOrigin2);

    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    bool subTensorsSupported = useSubtensor && workloadFactory.SupportsSubTensors();

    std::unique_ptr<armnn::ITensorHandle> inputHandle1 =
            subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo1.GetShape(), wOrigin1.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo1);

    std::unique_ptr<armnn::ITensorHandle> inputHandle2 =
            subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo2.GetShape(), wOrigin2.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo2);

    armnn::ConcatQueueDescriptor data;
    armnn::OriginsDescriptor desc = armnn::CreateDescriptorForConcatenation(
            inputTensorShapes.begin(),inputTensorShapes.end(), 2);
    data.m_Parameters = desc;

    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddInputToWorkload(data, info, inputTensorInfo2, inputHandle2.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    data.m_ViewOrigins.push_back(window1);
    data.m_ViewOrigins.push_back(window2);

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateConcat(data, info);

    inputHandle1->Allocate();
    inputHandle2->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0]);
    CopyDataToITensorHandle(inputHandle2.get(), &input2[0][0][0]);

    workload->PostAllocationConfigure();
    workload->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0], outputHandle.get());

    return ret;
}

//
// Explicit template specializations
//

template LayerTestResult<armnn::ResolveType<armnn::DataType::QuantisedAsymm8>, 3>
ConcatDifferentInputOutputQParamTest<armnn::DataType::QuantisedAsymm8>(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool useSubtensor);

template LayerTestResult<armnn::ResolveType<armnn::DataType::QuantisedSymm16>, 3>
ConcatDifferentInputOutputQParamTest<armnn::DataType::QuantisedSymm16>(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool useSubtensor);

//
// Implementation functions
//

LayerTestResult<float,3> ConcatTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    unsigned int outputWidth = 3;
    unsigned int outputHeight = 6;
    unsigned int outputChannels = 3;

    unsigned int inputWidth1 = 3;
    unsigned int inputHeight1 = 6;
    unsigned int inputChannels1 = 2;

    unsigned int inputWidth2 = 3;
    unsigned int inputHeight2 = 6;
    unsigned int inputChannels2 = 1;

    // Define the tensor descriptors.
    armnn::TensorInfo outputTensorInfo({ outputChannels, outputHeight, outputWidth }, armnn::DataType::Float32);
    armnn::TensorInfo inputTensorInfo1({ inputChannels1, inputHeight1, inputWidth1 }, armnn::DataType::Float32);
    armnn::TensorInfo inputTensorInfo2({ inputChannels2, inputHeight2, inputWidth2 }, armnn::DataType::Float32);

    LayerTestResult<float,3> ret(outputTensorInfo);

    ret.outputExpected = MakeTensor<float, 3>(outputTensorInfo, std::vector<float>(
    {
            1.0f, 2.0f, 3.0f,
            4.0f, 5.0f, 6.0f,
            7.0f, 8.0f, 9.0f,
            10.0f, 11.0f, 12.0f,
            13.0f, 14.0f, 15.0f,
            16.0f, 17.0f, 18.0f,

            19.0f, 20.0f, 21.0f,
            22.0f, 23.0f, 24.0f,
            25.0f, 26.0f, 27.0f,
            28.0f, 29.0f, 30.0f,
            31.0f, 32.0f, 33.0f,
            34.0f, 35.0f, 36.0f,

            37.0f, 38.0f, 39.0f,
            40.0f, 41.0f, 42.0f,
            43.0f, 44.0f, 45.0f,
            46.0f, 47.0f, 48.0f,
            49.0f, 50.0f, 51.0f,
            52.0f, 53.0f, 54.0f,
        })
    );

    auto input1 = MakeTensor<float, 3>(inputTensorInfo1, std::vector<float>(
        {
            1.0f, 2.0f, 3.0f,
            4.0f, 5.0f, 6.0f,
            7.0f, 8.0f, 9.0f,
            10.0f, 11.0f, 12.0f,
            13.0f, 14.0f, 15.0f,
            16.0f, 17.0f, 18.0f,

            19.0f, 20.0f, 21.0f,
            22.0f, 23.0f, 24.0f,
            25.0f, 26.0f, 27.0f,
            28.0f, 29.0f, 30.0f,
            31.0f, 32.0f, 33.0f,
            34.0f, 35.0f, 36.0f,
        })
    );

    auto input2 = MakeTensor<float, 3>(inputTensorInfo2, std::vector<float>(
        {
            37.0f, 38.0f, 39.0f,
            40.0f, 41.0f, 42.0f,
            43.0f, 44.0f, 45.0f,
            46.0f, 47.0f, 48.0f,
            49.0f, 50.0f, 51.0f,
            52.0f, 53.0f, 54.0f,
        })
    );

    std::vector<unsigned int> wOrigin1 = {0, 0, 0}; //Extent of the window is defined by size of input[0].
    armnn::ConcatQueueDescriptor::ViewOrigin window1(wOrigin1);

    std::vector<unsigned int> wOrigin2 = {2, 0, 0}; //Extent of the window is defined by size of input[1].
    armnn::ConcatQueueDescriptor::ViewOrigin window2(wOrigin2);

    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    bool subTensorsSupported = workloadFactory.SupportsSubTensors();

    std::unique_ptr<armnn::ITensorHandle> inputHandle1 =
        subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo1.GetShape(), wOrigin1.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo1);

    std::unique_ptr<armnn::ITensorHandle> inputHandle2  =
        subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo2.GetShape(), wOrigin2.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo2);

    armnn::ConcatQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddInputToWorkload(data, info, inputTensorInfo2, inputHandle2.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    data.m_ViewOrigins.push_back(window1);
    data.m_ViewOrigins.push_back(window2);

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateConcat(data, info);

    inputHandle1->Allocate();
    inputHandle2->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0]);
    CopyDataToITensorHandle(inputHandle2.get(), &input2[0][0][0]);

    workload->PostAllocationConfigure();
    workload->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0], outputHandle.get());

    return ret;
}

LayerTestResult<float, 1> Concat1dTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat1dTestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 2> Concat2dDim0Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat2dDim0TestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 2> Concat2dDim1Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat2dDim1TestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 2> Concat2dDim0DiffInputDimsTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat2dDim0DiffInputDimsTestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 2> Concat2dDim1DiffInputDimsTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat2dDim1DiffInputDimsTestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 3> Concat3dDim0Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat3dDim0TestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 3> Concat3dDim1Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat3dDim1TestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 3> Concat3dDim2Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool useSubtensor)
{
    return Concat3dDim2TestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, useSubtensor, 0.0f, 0);
}

LayerTestResult<float, 3> Concat3dDim0DiffInputDimsTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat3dDim0DiffInputDimsTestImpl<armnn::DataType::Float32>(
        workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 3> Concat3dDim1DiffInputDimsTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat3dDim1DiffInputDimsTestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 3> Concat3dDim2DiffInputDimsTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool useSubtensor)
{
    return Concat3dDim2DiffInputDimsTestImpl<armnn::DataType::Float32>(
        workloadFactory, memoryManager, useSubtensor, 0.0f, 0);
}

LayerTestResult<float, 4> Concat4dDim0Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat4dDim0TestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 4> Concat4dDim1Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat4dDim1TestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 4> Concat4dDim2Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat4dDim2TestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 4> Concat4dDim3Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool useSubtensor)
{
    return Concat4dDim3TestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0, useSubtensor);
}

LayerTestResult<float, 4> Concat4dDiffShapeDim0Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat4dDiffShapeDim0TestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 4> Concat4dDiffShapeDim1Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat4dDiffShapeDim1TestImpl<armnn::DataType::Float32>(
        workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 4> Concat4dDiffShapeDim2Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat4dDiffShapeDim2TestImpl<armnn::DataType::Float32>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<float, 4> Concat4dDiffShapeDim3Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool useSubtensor)
{
    return Concat4dDiffShapeDim3TestImpl<armnn::DataType::Float32>(
        workloadFactory, memoryManager, 0.0f, 0, useSubtensor);
}

LayerTestResult<armnn::Half, 3> ConcatFloat16Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat3dDim1TestImpl<armnn::DataType::Float16>(workloadFactory, memoryManager, 0.0f, 0);
}

LayerTestResult<uint8_t, 3> ConcatUint8DifferentQParamsTest(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    unsigned int outputWidth = 3;
    unsigned int outputHeight = 6;
    unsigned int outputChannels = 3;

    unsigned int inputWidth1 = 3;
    unsigned int inputHeight1 = 6;
    unsigned int inputChannels1 = 2;

    unsigned int inputWidth2 = 3;
    unsigned int inputHeight2 = 6;
    unsigned int inputChannels2 = 1;

    // Defines the tensor descriptors.
    armnn::TensorInfo outputTensorInfo({ outputChannels, outputHeight, outputWidth }, armnn::DataType::QuantisedAsymm8);
    armnn::TensorInfo inputTensorInfo1({ inputChannels1, inputHeight1, inputWidth1 }, armnn::DataType::QuantisedAsymm8);
    armnn::TensorInfo inputTensorInfo2({ inputChannels2, inputHeight2, inputWidth2 }, armnn::DataType::QuantisedAsymm8);

    // Quantized input1 tensor. Range [-3, 1]
    const float inputScale1 = 0.015686f;
    const int32_t inputOffset1 = 192;

    auto input1 = MakeTensor<uint8_t, 3>(inputTensorInfo1, std::vector<uint8_t>(
    {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9,
        10, 11, 12,
        13, 14, 15,
        16, 17, 18,

        19, 20, 21,
        22, 23, 24,
        25, 26, 27,
        28, 29, 30,
        31, 32, 33,
        34, 35, 36,
    })
    );

    // Quatized input2 tensor. Range [-1, 4]
    const float inputScale2 = 0.019608f;
    const int32_t inputOffset2 = 50;

    auto input2 = MakeTensor<uint8_t, 3>(inputTensorInfo2, std::vector<uint8_t>(
    {
        37, 38, 39,
        40, 41, 42,
        43, 44, 45,
        46, 47, 48,
        49, 50, 51,
        52, 53, 54,
    })
    );

    // Output has the same quantization parameters than input1,
    // so that only the requantization of input2 is required
    const float outputScale = 0.015686f;
    const int32_t outputOffset = 192;

    LayerTestResult<uint8_t, 3> ret(outputTensorInfo);

    ret.outputExpected = MakeTensor<uint8_t, 3>(outputTensorInfo, std::vector<uint8_t>(
    {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9,
        10, 11, 12,
        13, 14, 15,
        16, 17, 18,

        19, 20, 21,
        22, 23, 24,
        25, 26, 27,
        28, 29, 30,
        31, 32, 33,
        34, 35, 36,

        176, 177, 178,
        179, 181, 182,
        183, 184, 186,
        187, 188, 189,
        191, 192, 193,
        195, 196, 197,
    })
    );

    outputTensorInfo.SetQuantizationScale(outputScale);
    outputTensorInfo.SetQuantizationOffset(outputOffset);
    inputTensorInfo1.SetQuantizationScale(inputScale1);
    inputTensorInfo1.SetQuantizationOffset(inputOffset1);
    inputTensorInfo2.SetQuantizationScale(inputScale2);
    inputTensorInfo2.SetQuantizationOffset(inputOffset2);

    std::vector<unsigned int> wOrigin1 = { 0, 0, 0 }; //Extent of the window is defined by size of input[0].
    armnn::ConcatQueueDescriptor::ViewOrigin window1(wOrigin1);

    std::vector<unsigned int> wOrigin2 = { 2, 0, 0 }; //Extent of the window is defined by size of input[1].
    armnn::ConcatQueueDescriptor::ViewOrigin window2(wOrigin2);

    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    bool subTensorsSupported = workloadFactory.SupportsSubTensors();

    std::unique_ptr<armnn::ITensorHandle> inputHandle1 =
            subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo1.GetShape(), wOrigin1.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo1);

    std::unique_ptr<armnn::ITensorHandle> inputHandle2 =
            subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo2.GetShape(), wOrigin2.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo2);

    armnn::ConcatQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddInputToWorkload(data, info, inputTensorInfo2, inputHandle2.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    data.m_ViewOrigins.push_back(window1);
    data.m_ViewOrigins.push_back(window2);

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateConcat(data, info);

    inputHandle1->Allocate();
    inputHandle2->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0]);
    CopyDataToITensorHandle(inputHandle2.get(), &input2[0][0][0]);

    workload->PostAllocationConfigure();
    workload->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0], outputHandle.get());

    return ret;
}

LayerTestResult<uint8_t, 3> ConcatUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    unsigned int outputWidth = 3;
    unsigned int outputHeight = 6;
    unsigned int outputChannels = 3;

    unsigned int inputWidth1 = 3;
    unsigned int inputHeight1 = 6;
    unsigned int inputChannels1 = 2;

    unsigned int inputWidth2 = 3;
    unsigned int inputHeight2 = 6;
    unsigned int inputChannels2 = 1;

    // Defines the tensor descriptors.
    armnn::TensorInfo outputTensorInfo({ outputChannels, outputHeight, outputWidth }, armnn::DataType::QuantisedAsymm8);
    armnn::TensorInfo inputTensorInfo1({ inputChannels1, inputHeight1, inputWidth1 }, armnn::DataType::QuantisedAsymm8);
    armnn::TensorInfo inputTensorInfo2({ inputChannels2, inputHeight2, inputWidth2 }, armnn::DataType::QuantisedAsymm8);

    // Arbitrary scale and offsets. They don't really matter as the Concat operator doesn't dequantize/quantize them.
    const float scale = 0.13497836f;
    const int32_t offset = -7;

    outputTensorInfo.SetQuantizationScale(scale);
    outputTensorInfo.SetQuantizationOffset(offset);
    inputTensorInfo1.SetQuantizationScale(scale);
    inputTensorInfo1.SetQuantizationOffset(offset);
    inputTensorInfo2.SetQuantizationScale(scale);
    inputTensorInfo2.SetQuantizationOffset(offset);

    LayerTestResult<uint8_t, 3> ret(outputTensorInfo);

    ret.outputExpected = MakeTensor<uint8_t, 3>(outputTensorInfo, std::vector<uint8_t>(
        {
            1, 2, 3,
            4, 5, 6,
            7, 8, 9,
            10, 11, 12,
            13, 14, 15,
            16, 17, 18,

            19, 20, 21,
            22, 23, 24,
            25, 26, 27,
            28, 29, 30,
            31, 32, 33,
            34, 35, 36,

            37, 38, 39,
            40, 41, 42,
            43, 44, 45,
            46, 47, 48,
            49, 50, 51,
            52, 53, 54,
        })
    );

    auto input1 = MakeTensor<uint8_t, 3>(inputTensorInfo1, std::vector<uint8_t>(
    {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9,
        10, 11, 12,
        13, 14, 15,
        16, 17, 18,

        19, 20, 21,
        22, 23, 24,
        25, 26, 27,
        28, 29, 30,
        31, 32, 33,
        34, 35, 36,
    })
    );

    auto input2 = MakeTensor<uint8_t, 3>(inputTensorInfo2, std::vector<uint8_t>(
    {
        37, 38, 39,
        40, 41, 42,
        43, 44, 45,
        46, 47, 48,
        49, 50, 51,
        52, 53, 54,
    })
    );

    std::vector<unsigned int> wOrigin1 = { 0, 0, 0 }; //Extent of the window is defined by size of input[0].
    armnn::ConcatQueueDescriptor::ViewOrigin window1(wOrigin1);

    std::vector<unsigned int> wOrigin2 = { 2, 0, 0 }; //Extent of the window is defined by size of input[1].
    armnn::ConcatQueueDescriptor::ViewOrigin window2(wOrigin2);


    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    bool subTensorsSupported = workloadFactory.SupportsSubTensors();

    std::unique_ptr<armnn::ITensorHandle> inputHandle1 =
        subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo1.GetShape(), wOrigin1.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo1);

    std::unique_ptr<armnn::ITensorHandle> inputHandle2 =
        subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo2.GetShape(), wOrigin2.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo2);


    armnn::ConcatQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddInputToWorkload(data, info, inputTensorInfo2, inputHandle2.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    data.m_ViewOrigins.push_back(window1);
    data.m_ViewOrigins.push_back(window2);

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateConcat(data, info);

    inputHandle1->Allocate();
    inputHandle2->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0]);
    CopyDataToITensorHandle(inputHandle2.get(), &input2[0][0][0]);

    workload->PostAllocationConfigure();
    workload->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0], outputHandle.get());

    return ret;
}

LayerTestResult<uint16_t, 3> ConcatUint16Test(
        armnn::IWorkloadFactory& workloadFactory,
        const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    unsigned int outputWidth = 3;
    unsigned int outputHeight = 6;
    unsigned int outputChannels = 3;

    unsigned int inputWidth1 = 3;
    unsigned int inputHeight1 = 6;
    unsigned int inputChannels1 = 2;

    unsigned int inputWidth2 = 3;
    unsigned int inputHeight2 = 6;
    unsigned int inputChannels2 = 1;

    // Defines the tensor descriptors.
    armnn::TensorInfo outputTensorInfo({ outputChannels, outputHeight, outputWidth }, armnn::DataType::QuantisedSymm16);
    armnn::TensorInfo inputTensorInfo1({ inputChannels1, inputHeight1, inputWidth1 }, armnn::DataType::QuantisedSymm16);
    armnn::TensorInfo inputTensorInfo2({ inputChannels2, inputHeight2, inputWidth2 }, armnn::DataType::QuantisedSymm16);

    // Arbitrary scale and offsets. They don't really matter as the Concat operator doesn't dequantize/quantize them.
    const float scale = 0.13497836f;
    const int32_t offset = -7;

    outputTensorInfo.SetQuantizationScale(scale);
    outputTensorInfo.SetQuantizationOffset(offset);
    inputTensorInfo1.SetQuantizationScale(scale);
    inputTensorInfo1.SetQuantizationOffset(offset);
    inputTensorInfo2.SetQuantizationScale(scale);
    inputTensorInfo2.SetQuantizationOffset(offset);

    LayerTestResult<uint16_t, 3> ret(outputTensorInfo);

    ret.outputExpected = MakeTensor<uint16_t, 3>(outputTensorInfo, std::vector<uint16_t>(
    {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9,
        10, 11, 12,
        13, 14, 15,
        16, 17, 18,

        19, 20, 21,
        22, 23, 24,
        25, 26, 27,
        28, 29, 30,
        31, 32, 33,
        34, 35, 36,

        37, 38, 39,
        40, 41, 42,
        43, 44, 45,
        46, 47, 48,
        49, 50, 51,
        52, 53, 54,
    }));

    auto input1 = MakeTensor<uint16_t, 3>(inputTensorInfo1, std::vector<uint16_t>(
    {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9,
        10, 11, 12,
        13, 14, 15,
        16, 17, 18,

        19, 20, 21,
        22, 23, 24,
        25, 26, 27,
        28, 29, 30,
        31, 32, 33,
        34, 35, 36,
    }));

    auto input2 = MakeTensor<uint16_t, 3>(inputTensorInfo2, std::vector<uint16_t>(
    {
        37, 38, 39,
        40, 41, 42,
        43, 44, 45,
        46, 47, 48,
        49, 50, 51,
        52, 53, 54,
    }));

    std::vector<unsigned int> wOrigin1 = { 0, 0, 0 }; //Extent of the window is defined by size of input[0].
    armnn::ConcatQueueDescriptor::ViewOrigin window1(wOrigin1);

    std::vector<unsigned int> wOrigin2 = { 2, 0, 0 }; //Extent of the window is defined by size of input[1].
    armnn::ConcatQueueDescriptor::ViewOrigin window2(wOrigin2);


    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    bool subTensorsSupported = workloadFactory.SupportsSubTensors();

    std::unique_ptr<armnn::ITensorHandle> inputHandle1 =
            subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo1.GetShape(), wOrigin1.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo1);

    std::unique_ptr<armnn::ITensorHandle> inputHandle2 =
            subTensorsSupported ?
            workloadFactory.CreateSubTensorHandle(*outputHandle, inputTensorInfo2.GetShape(), wOrigin2.data()) :
            workloadFactory.CreateTensorHandle(inputTensorInfo2);


    armnn::ConcatQueueDescriptor data;
    armnn::WorkloadInfo info;
    AddInputToWorkload(data, info, inputTensorInfo1, inputHandle1.get());
    AddInputToWorkload(data, info, inputTensorInfo2, inputHandle2.get());
    AddOutputToWorkload(data, info, outputTensorInfo, outputHandle.get());

    data.m_ViewOrigins.push_back(window1);
    data.m_ViewOrigins.push_back(window2);

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateConcat(data, info);

    inputHandle1->Allocate();
    inputHandle2->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle1.get(), &input1[0][0][0]);
    CopyDataToITensorHandle(inputHandle2.get(), &input2[0][0][0]);

    workload->PostAllocationConfigure();
    workload->Execute();

    CopyDataFromITensorHandle(&ret.output[0][0][0], outputHandle.get());

    return ret;
}

LayerTestResult<uint8_t, 1> Concat1dUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat1dTestImpl<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 2> Concat2dDim0Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat2dDim0TestImpl<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 2> Concat2dDim1Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat2dDim1TestImpl<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 2> Concat2dDim0DiffInputDimsUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat2dDim0DiffInputDimsTestImpl<armnn::DataType::QuantisedAsymm8>(
        workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 2> Concat2dDim1DiffInputDimsUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat2dDim1DiffInputDimsTestImpl<armnn::DataType::QuantisedAsymm8>(
        workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 3> Concat3dDim0Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat3dDim0TestImpl<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 3> Concat3dDim1Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat3dDim1TestImpl<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 3> Concat3dDim2Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool useSubtensor)
{
    return Concat3dDim2TestImpl<armnn::DataType::QuantisedAsymm8>(
        workloadFactory, memoryManager, useSubtensor, 0.5f, -1);
}

LayerTestResult<uint8_t, 3> Concat3dDim0DiffInputDimsUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat3dDim0TestImpl<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 3> Concat3dDim1DiffInputDimsUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat3dDim1DiffInputDimsTestImpl<armnn::DataType::QuantisedAsymm8>(
        workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 3> Concat3dDim2DiffInputDimsUint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool useSubtensor)
{
    return Concat3dDim2DiffInputDimsTestImpl<armnn::DataType::QuantisedAsymm8>(
        workloadFactory, memoryManager, useSubtensor, 0.5f, -1);
}

LayerTestResult<uint8_t, 4> Concat4dDim0Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat4dDim0TestImpl<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 4> Concat4dDim1Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat4dDim1TestImpl<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 4> Concat4dDim2Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat4dDim2TestImpl<armnn::DataType::QuantisedAsymm8>(workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 4> Concat4dDim3Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager, bool useSubtensor)
{
    return Concat4dDim3TestImpl<armnn::DataType::QuantisedAsymm8>(
        workloadFactory, memoryManager, 0.5f, -1, useSubtensor);
}

LayerTestResult<uint8_t, 4> Concat4dDiffShapeDim0Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat4dDiffShapeDim0TestImpl<armnn::DataType::QuantisedAsymm8>(
        workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 4> Concat4dDiffShapeDim1Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat4dDiffShapeDim1TestImpl<armnn::DataType::QuantisedAsymm8>(
        workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 4> Concat4dDiffShapeDim2Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager)
{
    return Concat4dDiffShapeDim2TestImpl<armnn::DataType::QuantisedAsymm8>(
        workloadFactory, memoryManager, 0.5f, -1);
}

LayerTestResult<uint8_t, 4> Concat4dDiffShapeDim3Uint8Test(
    armnn::IWorkloadFactory& workloadFactory,
    const armnn::IBackendInternal::IMemoryManagerSharedPtr& memoryManager,
    bool useSubtensor)
{
    return Concat4dDiffShapeDim3TestImpl<armnn::DataType::QuantisedAsymm8>(
        workloadFactory, memoryManager, 0.5f, -1, useSubtensor);
}
