﻿//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//
#include <backends/CpuTensorHandle.hpp>
#include <backends/MemCopyWorkload.hpp>
#include <backends/MakeWorkloadHelper.hpp>
#include "RefWorkloadFactory.hpp"
#include "workloads/RefWorkloads.hpp"
#include "Layer.hpp"

#include <boost/log/trivial.hpp>

namespace armnn
{

template <typename F32Workload, typename U8Workload, typename QueueDescriptorType>
std::unique_ptr<IWorkload> RefWorkloadFactory::MakeWorkload(const QueueDescriptorType& descriptor,
    const WorkloadInfo& info) const
{
    return armnn::MakeWorkload<NullWorkload, F32Workload, U8Workload>(descriptor, info);
}

RefWorkloadFactory::RefWorkloadFactory()
{
}

bool RefWorkloadFactory::IsLayerSupported(const Layer& layer, boost::optional<DataType> dataType,
                                          std::string& outReasonIfUnsupported)
{
    return IWorkloadFactory::IsLayerSupported(Compute::CpuRef, layer, dataType, outReasonIfUnsupported);
}

std::unique_ptr<ITensorHandle> RefWorkloadFactory::CreateTensorHandle(const TensorInfo& tensorInfo) const
{
    return std::make_unique<ScopedCpuTensorHandle>(tensorInfo);
}

std::unique_ptr<ITensorHandle> RefWorkloadFactory::CreateTensorHandle(const TensorInfo& tensorInfo,
                                                                      DataLayout dataLayout) const
{
    return std::make_unique<ScopedCpuTensorHandle>(tensorInfo);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateInput(const InputQueueDescriptor& descriptor,
                                                           const WorkloadInfo& info) const
{
    if (info.m_InputTensorInfos.empty() )
    {
        throw InvalidArgumentException("RefWorkloadFactory::CreateInput: Input cannot be zero length");
    }
    if (info.m_OutputTensorInfos.empty())
    {
        throw InvalidArgumentException("RefWorkloadFactory::CreateInput: Output cannot be zero length");
    }

    if (info.m_InputTensorInfos[0].GetNumBytes() != info.m_OutputTensorInfos[0].GetNumBytes())
    {
        throw InvalidArgumentException("RefWorkloadFactory::CreateInput: data input and output differ in byte count.");
    }

    return MakeWorkload<CopyMemGenericWorkload, CopyMemGenericWorkload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateOutput(const OutputQueueDescriptor& descriptor,
                                                            const WorkloadInfo& info) const
{
    if (info.m_InputTensorInfos.empty() )
    {
        throw InvalidArgumentException("RefWorkloadFactory::CreateOutput: Input cannot be zero length");
    }
    if (info.m_OutputTensorInfos.empty())
    {
        throw InvalidArgumentException("RefWorkloadFactory::CreateOutput: Output cannot be zero length");
    }
    if (info.m_InputTensorInfos[0].GetNumBytes() != info.m_OutputTensorInfos[0].GetNumBytes())
    {
        throw InvalidArgumentException("RefWorkloadFactory::CreateOutput: data input and output differ in byte count.");
    }

    return MakeWorkload<CopyMemGenericWorkload, CopyMemGenericWorkload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateActivation(const ActivationQueueDescriptor& descriptor,
                                                                const WorkloadInfo&              info) const
{
    return MakeWorkload<RefActivationFloat32Workload, RefActivationUint8Workload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateSoftmax(const SoftmaxQueueDescriptor& descriptor,
                                                             const WorkloadInfo&           info) const
{
    return MakeWorkload<RefSoftmaxFloat32Workload, RefSoftmaxUint8Workload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateSplitter(const SplitterQueueDescriptor& descriptor,
                                                              const WorkloadInfo&            info) const
{
    return MakeWorkload<RefSplitterFloat32Workload, RefSplitterUint8Workload>(descriptor, info);
}

std::unique_ptr<armnn::IWorkload> RefWorkloadFactory::CreateMerger(const MergerQueueDescriptor& descriptor,
                                                                   const WorkloadInfo&          info) const
{
    return MakeWorkload<RefMergerFloat32Workload, RefMergerUint8Workload>(descriptor, info);
}

std::unique_ptr<armnn::IWorkload> RefWorkloadFactory::CreateFullyConnected(
    const FullyConnectedQueueDescriptor& descriptor, const WorkloadInfo& info) const
{
    return MakeWorkload<RefFullyConnectedFloat32Workload, RefFullyConnectedUint8Workload>(descriptor, info);
}

std::unique_ptr<armnn::IWorkload> RefWorkloadFactory::CreatePermute(const PermuteQueueDescriptor& descriptor,
                                                                    const WorkloadInfo&           info) const
{
    return armnn::MakeWorkload<RefPermuteFloat16Workload, RefPermuteFloat32Workload, RefPermuteUint8Workload>
        (descriptor, info);
}

std::unique_ptr<armnn::IWorkload> RefWorkloadFactory::CreatePooling2d(const Pooling2dQueueDescriptor& descriptor,
                                                                      const WorkloadInfo&           info) const
{
    return MakeWorkload<RefPooling2dFloat32Workload, RefPooling2dUint8Workload>(descriptor, info);
}

std::unique_ptr<armnn::IWorkload> RefWorkloadFactory::CreateConvolution2d(
    const Convolution2dQueueDescriptor& descriptor, const WorkloadInfo& info) const
{
    return MakeWorkload<RefConvolution2dFloat32Workload, RefConvolution2dUint8Workload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateDepthwiseConvolution2d(
    const DepthwiseConvolution2dQueueDescriptor& descriptor, const WorkloadInfo& info) const
{
    return MakeWorkload<RefDepthwiseConvolution2dFloat32Workload,
        RefDepthwiseConvolution2dUint8Workload>(descriptor, info);
}

std::unique_ptr<armnn::IWorkload> RefWorkloadFactory::CreateNormalization(
    const NormalizationQueueDescriptor& descriptor, const WorkloadInfo& info) const
{
    return MakeWorkload<RefNormalizationFloat32Workload, NullWorkload>(descriptor, info);
}

std::unique_ptr<armnn::IWorkload> RefWorkloadFactory::CreateAddition(const AdditionQueueDescriptor& descriptor,
                                                                     const WorkloadInfo&            info) const
{
    return MakeWorkload<RefAdditionFloat32Workload, RefAdditionUint8Workload>(descriptor, info);
}

std::unique_ptr<armnn::IWorkload> RefWorkloadFactory::CreateMultiplication(
    const MultiplicationQueueDescriptor& descriptor, const WorkloadInfo& info) const
{
    return MakeWorkload<RefMultiplicationFloat32Workload, RefMultiplicationUint8Workload>(descriptor, info);
}

std::unique_ptr<armnn::IWorkload> RefWorkloadFactory::CreateBatchNormalization(
    const BatchNormalizationQueueDescriptor& descriptor, const WorkloadInfo& info) const
{
    return MakeWorkload<RefBatchNormalizationFloat32Workload, RefBatchNormalizationUint8Workload>(descriptor, info);
}

std::unique_ptr<armnn::IWorkload> RefWorkloadFactory::CreateMemCopy(const MemCopyQueueDescriptor& descriptor,
                                                                    const WorkloadInfo&        info) const
{
    if (descriptor.m_Inputs.empty())
    {
        throw InvalidArgumentException("RefWorkloadFactory: CreateMemCopy() expected an input tensor.");
    }
    return std::make_unique<CopyMemGenericWorkload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateResizeBilinear(const ResizeBilinearQueueDescriptor& descriptor,
                                                                    const WorkloadInfo& info) const
{
    return MakeWorkload<RefResizeBilinearFloat32Workload, RefResizeBilinearUint8Workload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateFakeQuantization(
    const FakeQuantizationQueueDescriptor& descriptor,
    const WorkloadInfo& info) const
{
    return MakeWorkload<RefFakeQuantizationFloat32Workload, NullWorkload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateL2Normalization(const L2NormalizationQueueDescriptor& descriptor,
    const WorkloadInfo& info) const
{
    return MakeWorkload<RefL2NormalizationFloat32Workload, NullWorkload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateConstant(const ConstantQueueDescriptor& descriptor,
    const WorkloadInfo& info) const
{
    return MakeWorkload<RefConstantFloat32Workload, RefConstantUint8Workload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateReshape(const ReshapeQueueDescriptor& descriptor,
    const WorkloadInfo& info) const
{
    return MakeWorkload<RefReshapeFloat32Workload, RefReshapeUint8Workload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateFloor(const FloorQueueDescriptor& descriptor,
                                                          const WorkloadInfo& info) const
{
    return MakeWorkload<RefFloorFloat32Workload, NullWorkload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateLstm(const LstmQueueDescriptor& descriptor,
    const WorkloadInfo& info) const
{
    return MakeWorkload<RefLstmFloat32Workload, NullWorkload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateConvertFp16ToFp32(
    const ConvertFp16ToFp32QueueDescriptor& descriptor,
    const WorkloadInfo& info) const
{
    return std::make_unique<RefConvertFp16ToFp32Workload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreateConvertFp32ToFp16(
    const ConvertFp32ToFp16QueueDescriptor& descriptor,
    const WorkloadInfo& info) const
{
    return std::make_unique<RefConvertFp32ToFp16Workload>(descriptor, info);
}

std::unique_ptr<armnn::IWorkload> RefWorkloadFactory::CreateDivision(
    const DivisionQueueDescriptor& descriptor, const WorkloadInfo& info) const
{
    return MakeWorkload<RefDivisionFloat32Workload, RefDivisionUint8Workload>(descriptor, info);
}

std::unique_ptr<armnn::IWorkload> RefWorkloadFactory::CreateSubtraction(
    const SubtractionQueueDescriptor& descriptor, const WorkloadInfo& info) const
{
    return MakeWorkload<RefSubtractionFloat32Workload, RefSubtractionUint8Workload>(descriptor, info);
}

std::unique_ptr<armnn::IWorkload> RefWorkloadFactory::CreateMean(
    const MeanQueueDescriptor& descriptor, const WorkloadInfo& info) const
{
    return MakeWorkload<RefMeanFloat32Workload, RefMeanUint8Workload>(descriptor, info);
}

std::unique_ptr<IWorkload> RefWorkloadFactory::CreatePad(const PadQueueDescriptor& descriptor,
                                                 const WorkloadInfo& info) const
{
    return MakeWorkload<NullWorkload, NullWorkload>(descriptor, info);
}


} // namespace armnn