﻿//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once

#include <armnn/Optional.hpp>
#include <backendsCommon/WorkloadFactory.hpp>
#include <backendsCommon/OutputHandler.hpp>

#include "RefMemoryManager.hpp"

#include <boost/core/ignore_unused.hpp>


namespace armnn
{

template <typename QueueDescriptorType>
constexpr bool IsOperationQueueDescriptor(const QueueDescriptorType&) { return true; }

template <>
constexpr bool IsOperationQueueDescriptor(const MemCopyQueueDescriptor&) { return false; }

template <>
constexpr bool IsOperationQueueDescriptor(const ConstantQueueDescriptor&) { return false; }

template <>
constexpr bool IsOperationQueueDescriptor(const PermuteQueueDescriptor&) { return false; }

// Reference workload factory.
class RefWorkloadFactory : public IWorkloadFactory
{
public:
    explicit RefWorkloadFactory(const std::shared_ptr<RefMemoryManager>& memoryManager);
    RefWorkloadFactory();

    ~RefWorkloadFactory() {}

    const BackendId& GetBackendId() const override;

    static bool IsLayerSupported(const Layer& layer,
                                 Optional<DataType> dataType,
                                 std::string& outReasonIfUnsupported);

    bool SupportsSubTensors() const override { return false; }

    std::unique_ptr<ITensorHandle> CreateSubTensorHandle(ITensorHandle& parent,
                                                         TensorShape const& subTensorShape,
                                                         unsigned int const* subTensorOrigin) const override
    {
        boost::ignore_unused(parent, subTensorShape, subTensorOrigin);
        return nullptr;
    }

    std::unique_ptr<ITensorHandle> CreateTensorHandle(const TensorInfo& tensorInfo,
                                                      const bool IsMemoryManaged = true) const override;

    std::unique_ptr<ITensorHandle> CreateTensorHandle(const TensorInfo& tensorInfo,
                                                      DataLayout dataLayout,
                                                      const bool IsMemoryManaged = true) const override;

    std::unique_ptr<IWorkload> CreateInput(const InputQueueDescriptor& descriptor,
                                           const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateOutput(const OutputQueueDescriptor& descriptor,
                                            const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateActivation(const ActivationQueueDescriptor& descriptor,
                                                const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateArgMinMax(const ArgMinMaxQueueDescriptor& descriptor,
                                               const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateSoftmax(const SoftmaxQueueDescriptor& descriptor,
                                             const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateSplitter(const SplitterQueueDescriptor& descriptor,
                                              const WorkloadInfo& info) const override;

    ARMNN_DEPRECATED_MSG("Use CreateConcat instead")
    std::unique_ptr<IWorkload> CreateMerger(const MergerQueueDescriptor& descriptor,
                                            const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateFullyConnected(const FullyConnectedQueueDescriptor& descriptor,
                                                    const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreatePooling2d(const Pooling2dQueueDescriptor& descriptor,
                                               const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreatePermute(const PermuteQueueDescriptor& descriptor,
                                             const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateConvolution2d(const Convolution2dQueueDescriptor& descriptor,
                                                   const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateDepthToSpace(const DepthToSpaceQueueDescriptor& descriptor,
                                                  const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateDepthwiseConvolution2d(const DepthwiseConvolution2dQueueDescriptor& descriptor,
                                                            const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateDetectionPostProcess(const DetectionPostProcessQueueDescriptor& descriptor,
                                                          const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateNormalization(const NormalizationQueueDescriptor& descriptor,
                                                   const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateMultiplication(const MultiplicationQueueDescriptor& descriptor,
                                                    const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateAddition(const AdditionQueueDescriptor& descriptor,
                                              const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateBatchNormalization(const BatchNormalizationQueueDescriptor& descriptor,
                                                        const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateMemCopy(const MemCopyQueueDescriptor& descriptor,
                                             const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateMemImport(const MemImportQueueDescriptor& descriptor,
                                                const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateResize(const ResizeQueueDescriptor& descriptor,
                                            const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateResizeBilinear(const ResizeBilinearQueueDescriptor& descriptor,
                                                    const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateFakeQuantization(const FakeQuantizationQueueDescriptor& descriptor,
                                                      const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateL2Normalization(const L2NormalizationQueueDescriptor& descriptor,
                                                     const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateConcat(const ConcatQueueDescriptor& descriptor,
                                            const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateConstant(const ConstantQueueDescriptor& descriptor,
                                              const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateReshape(const ReshapeQueueDescriptor& descriptor,
                                             const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateSpaceToBatchNd(const SpaceToBatchNdQueueDescriptor& descriptor,
                                                    const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateSpaceToDepth(const SpaceToDepthQueueDescriptor& descriptor,
                                                  const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateFloor(const FloorQueueDescriptor& descriptor,
                                           const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateLstm(const LstmQueueDescriptor& descriptor,
                                          const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateConvertFp16ToFp32(const ConvertFp16ToFp32QueueDescriptor& descriptor,
                                                       const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateConvertFp32ToFp16(const ConvertFp32ToFp16QueueDescriptor& descriptor,
                                                       const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateDivision(const DivisionQueueDescriptor& descriptor,
                                              const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateSubtraction(const SubtractionQueueDescriptor& descriptor,
                                                 const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateMaximum(const MaximumQueueDescriptor& descriptor,
                                             const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateMean(const MeanQueueDescriptor& descriptor,
                                          const WorkloadInfo& Info) const override;

    std::unique_ptr<IWorkload> CreatePad(const PadQueueDescriptor& descriptor,
                                         const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateEqual(const EqualQueueDescriptor& descriptor,
                                           const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateBatchToSpaceNd(const BatchToSpaceNdQueueDescriptor& descriptor,
                                                    const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateStridedSlice(const StridedSliceQueueDescriptor& descriptor,
                                                  const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateMinimum(const MinimumQueueDescriptor& descriptor,
                                             const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateGreater(const GreaterQueueDescriptor& descriptor,
                                             const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateDebug(const DebugQueueDescriptor& descriptor,
                                           const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateRsqrt(const RsqrtQueueDescriptor& descriptor,
                                           const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreatePreCompiled(const PreCompiledQueueDescriptor& descriptor,
                                                 const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateGather(const GatherQueueDescriptor& descriptor,
                                            const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateDequantize(const DequantizeQueueDescriptor& descriptor,
                                                const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateQuantize(const QuantizeQueueDescriptor& descriptor,
                                              const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreatePrelu(const PreluQueueDescriptor& descriptor,
                                           const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateTransposeConvolution2d(const TransposeConvolution2dQueueDescriptor& descriptor,
                                                            const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateStack(const StackQueueDescriptor& descriptor,
                                           const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateAbs(const AbsQueueDescriptor& descriptor,
                                         const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateSlice(const SliceQueueDescriptor& descriptor,
                                           const WorkloadInfo& info) const override;

    std::unique_ptr<IWorkload> CreateInstanceNormalization(const InstanceNormalizationQueueDescriptor& descriptor,
                                                           const WorkloadInfo& info) const override;

private:

    template <typename F32Workload, typename U8Workload, typename QueueDescriptorType>
    std::unique_ptr<IWorkload> MakeWorkload(const QueueDescriptorType& descriptor, const WorkloadInfo& info) const;

    mutable std::shared_ptr<RefMemoryManager> m_MemoryManager;
};

} // namespace armnn
