﻿//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "CpuTensorHandle.hpp"
#include "WorkloadFactory.hpp"


#include <Layer.hpp>
#include <LayersFwd.hpp>

#include <armnn/Types.hpp>
#include <armnn/LayerSupport.hpp>
#include <armnn/ILayerSupport.hpp>

#include <backendsCommon/BackendRegistry.hpp>
#include <backendsCommon/WorkloadFactory.hpp>
#include <backendsCommon/IBackendInternal.hpp>
#include <backendsCommon/test/WorkloadTestUtils.hpp>

#include <boost/cast.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <cstring>
#include <sstream>

namespace armnn
{

namespace
{

const TensorInfo OverrideDataType(const TensorInfo& info, Optional<DataType> type)
{
    if (!type)
    {
        return info;
    }

    return TensorInfo(info.GetShape(), type.value(), info.GetQuantizationScale(), info.GetQuantizationOffset());
}

} // anonymous namespace

bool IWorkloadFactory::IsLayerSupported(const BackendId& backendId,
                                        const IConnectableLayer& connectableLayer,
                                        Optional<DataType> dataType,
                                        std::string& outReasonIfUnsupported)
{
    Optional<std::string&> reason = outReasonIfUnsupported;
    bool result;
    const Layer& layer = *(boost::polymorphic_downcast<const Layer*>(&connectableLayer));

    auto const& backendRegistry = BackendRegistryInstance();
    if (!backendRegistry.IsBackendRegistered(backendId))
    {
        std::stringstream ss;
        ss << connectableLayer.GetName() << " is not supported on " << backendId
           << " because this backend is not registered.";

        outReasonIfUnsupported = ss.str();
        return false;
    }

    auto backendFactory = backendRegistry.GetFactory(backendId);
    auto backendObject = backendFactory();
    auto layerSupportObject = backendObject->GetLayerSupport();

    switch(layer.GetType())
    {
        case LayerType::Abs:
        {
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsAbsSupported(OverrideDataType(input, dataType),
                                                        OverrideDataType(output, dataType),
                                                        reason);
            break;
        }
        case LayerType::Activation:
        {
            auto cLayer = boost::polymorphic_downcast<const ActivationLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsActivationSupported(
                                           OverrideDataType(input, dataType),
                                           OverrideDataType(output, dataType),
                                           cLayer->GetParameters(),
                                           reason);
            break;
        }
        case LayerType::Addition:
        {
            const TensorInfo& input0 = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& input1 = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsAdditionSupported(
                                        OverrideDataType(input0, dataType),
                                        OverrideDataType(input1, dataType),
                                        OverrideDataType(output, dataType),
                                        reason);
            break;
        }
        case LayerType::ArgMinMax:
        {
            auto cLayer = boost::polymorphic_downcast<const ArgMinMaxLayer*>(&layer);
            const ArgMinMaxDescriptor& descriptor = cLayer->GetParameters();

            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsArgMinMaxSupported(
                    OverrideDataType(input, dataType),
                    OverrideDataType(output, dataType),
                    descriptor,
                    reason);
            break;
        }
        case LayerType::BatchNormalization:
        {
            auto cLayer = boost::polymorphic_downcast<const BatchNormalizationLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            const TensorInfo& mean = cLayer->m_Mean->GetTensorInfo();
            const TensorInfo& var = cLayer->m_Variance->GetTensorInfo();
            const TensorInfo& beta = cLayer->m_Beta->GetTensorInfo();
            const TensorInfo& gamma = cLayer->m_Gamma->GetTensorInfo();
            result = layerSupportObject->IsBatchNormalizationSupported(
                                                   OverrideDataType(input, dataType),
                                                   OverrideDataType(output, dataType),
                                                   OverrideDataType(mean, dataType),
                                                   OverrideDataType(var, dataType),
                                                   OverrideDataType(beta, dataType),
                                                   OverrideDataType(gamma, dataType),
                                                   cLayer->GetParameters(),
                                                   reason);
            break;
        }
        case LayerType::BatchToSpaceNd:
        {
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            auto cLayer = boost::polymorphic_downcast<const BatchToSpaceNdLayer*>(&layer);

            result = layerSupportObject->IsBatchToSpaceNdSupported(OverrideDataType(input, dataType),
                                                                   OverrideDataType(output, dataType),
                                                                   cLayer->GetParameters(),
                                                                   reason);
            break;
        }
        case LayerType::Constant:
        {
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsConstantSupported(OverrideDataType(output, dataType), reason);
            break;
        }
        case LayerType::ConvertFp16ToFp32:
        {
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsConvertFp16ToFp32Supported(input, output, reason);
            break;
        }
        case LayerType::ConvertFp32ToFp16:
        {
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsConvertFp32ToFp16Supported(input, output, reason);
            break;
        }
        case LayerType::Convolution2d:
        {
            auto cLayer = boost::polymorphic_downcast<const Convolution2dLayer*>(&layer);

            const TensorInfo input  = OverrideDataType(layer.GetInputSlot(0).GetConnection()->GetTensorInfo(),
                                                       dataType);
            const TensorInfo output = OverrideDataType(layer.GetOutputSlot(0).GetTensorInfo(), dataType);
            BOOST_ASSERT(cLayer->m_Weight.get() != nullptr);

            const Convolution2dDescriptor& descriptor  = cLayer->GetParameters();

            // Construct optional biases object based on the value of m_BiasEnabled
            Optional<TensorInfo> biases;
            if (descriptor.m_BiasEnabled)
            {
                biases =
                    OverrideDataType(cLayer->m_Bias->GetTensorInfo(), GetBiasTypeFromWeightsType(dataType));
            }

            result = layerSupportObject->IsConvolution2dSupported(
                                              input,
                                              output,
                                              descriptor,
                                              OverrideDataType(cLayer->m_Weight->GetTensorInfo(), dataType),
                                              biases,
                                              reason);
            break;
        }
        case LayerType::Debug:
        {
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();

            result = layerSupportObject->IsDebugSupported(OverrideDataType(input, dataType),
                                                          OverrideDataType(output, dataType),
                                                          reason);
            break;
        }
        case LayerType::DepthToSpace:
        {
            auto cLayer = boost::polymorphic_downcast<const DepthToSpaceLayer*>(&layer);

            const TensorInfo& input  = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();

            result = layerSupportObject->IsDepthToSpaceSupported(OverrideDataType(input, dataType),
                                                                 OverrideDataType(output, dataType),
                                                                 cLayer->GetParameters(),
                                                                 reason);
            break;
        }
        case LayerType::DepthwiseConvolution2d:
        {
            auto cLayer = boost::polymorphic_downcast<const DepthwiseConvolution2dLayer*>(&layer);
            const TensorInfo& input = OverrideDataType(layer.GetInputSlot(0).GetConnection()->GetTensorInfo(),
                                                       dataType);
            const TensorInfo& output = OverrideDataType(layer.GetOutputSlot(0).GetTensorInfo(), dataType);
            BOOST_ASSERT(cLayer->m_Weight.get() != nullptr);

            const DepthwiseConvolution2dDescriptor& descriptor = cLayer->GetParameters();

            // Construct optional biases object based on the value of m_BiasEnabled
            Optional<TensorInfo> biases;
            if (descriptor.m_BiasEnabled)
            {
                biases =
                    OverrideDataType(cLayer->m_Bias->GetTensorInfo(), GetBiasTypeFromWeightsType(dataType));
            }

            result = layerSupportObject->IsDepthwiseConvolutionSupported(
                                                     input,
                                                     output,
                                                     descriptor,
                                                     OverrideDataType(cLayer->m_Weight->GetTensorInfo(), dataType),
                                                     biases,
                                                     reason);
            break;
        }
        case LayerType::Dequantize:
        {
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();

            result = layerSupportObject->IsDequantizeSupported(OverrideDataType(input, dataType),
                                                               OverrideDataType(output, DataType::Float32),
                                                               reason);
            break;
        }
        case LayerType::DetectionPostProcess:
        {
            const TensorInfo& input0 = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& input1 = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            auto cLayer = boost::polymorphic_downcast<const DetectionPostProcessLayer*>(&layer);
            const DetectionPostProcessDescriptor& descriptor = cLayer->GetParameters();
            result = layerSupportObject->IsDetectionPostProcessSupported(input0,
                                                                         input1,
                                                                         descriptor,
                                                                         reason);
            break;
        }
        case LayerType::Equal:
        {
            const TensorInfo& input0 = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& input1 = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsEqualSupported(OverrideDataType(input0, dataType),
                                                          OverrideDataType(input1, dataType),
                                                          OverrideDataType(output, dataType),
                                                          reason);
            break;
        }
        case LayerType::FakeQuantization:
        {
            auto cLayer = boost::polymorphic_downcast<const FakeQuantizationLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            result = layerSupportObject->IsFakeQuantizationSupported(OverrideDataType(input, dataType),
                                                                     cLayer->GetParameters(),
                                                                     reason);
            break;
        }
        case LayerType::Floor:
        {
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsFloorSupported(OverrideDataType(input, dataType),
                                                          OverrideDataType(output, dataType),
                                                          reason);
            break;
        }
        case LayerType::FullyConnected:
        {
            auto cLayer = boost::polymorphic_downcast<const FullyConnectedLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            BOOST_ASSERT(cLayer->m_Weight.get() != nullptr);

            TensorInfo biasInfo;
            const TensorInfo * biasInfoPtr = nullptr;
            static const TensorInfo dummyFloat16Bias(TensorShape({1,1,1,1}), DataType::Float16);
            static const TensorInfo dummyFloat32Bias(TensorShape({1,1,1,1}), DataType::Float32);
            static const TensorInfo dummyQA8Bias(TensorShape({1,1,1,1}), DataType::Signed32);

            const FullyConnectedDescriptor& descriptor = cLayer->GetParameters();
            if (descriptor.m_BiasEnabled)
            {
                BOOST_ASSERT(cLayer->m_Bias.get() != nullptr);
                biasInfo = OverrideDataType(cLayer->m_Bias->GetTensorInfo(), GetBiasTypeFromWeightsType(dataType));
                biasInfoPtr = &biasInfo;
            }
            else
            {
                // If biases are not enabled pass a dummy tensorinfo for the validation
                switch(input.GetDataType())
                {
                    case DataType::Float16:
                    {
                        biasInfoPtr = &dummyFloat16Bias;
                        break;
                    }
                    case DataType::Float32:
                    {
                        biasInfoPtr = &dummyFloat32Bias;
                        break;
                    }
                    case DataType::QuantisedAsymm8:
                    case DataType::QuantisedSymm16:
                    {
                        biasInfoPtr = &dummyQA8Bias;
                        break;
                    }
                    default:
                    {
                        BOOST_ASSERT_MSG(false, "Unexpected bias type");
                    }
                }
            }

            result = layerSupportObject->IsFullyConnectedSupported(
                                               OverrideDataType(input, dataType),
                                               OverrideDataType(output, dataType),
                                               OverrideDataType(cLayer->m_Weight->GetTensorInfo(), dataType),
                                               *biasInfoPtr,
                                               descriptor,
                                               reason);
            break;
        }
        case LayerType::Gather:
        {
            const TensorInfo& input0 = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& input1 = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsGatherSupported(OverrideDataType(input0, dataType),
                                                           input1,
                                                           OverrideDataType(output, dataType),
                                                           reason);
            break;
        }
        case LayerType::Input:
        {
            const TensorInfo& input = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsInputSupported(OverrideDataType(input, dataType), reason);
            break;
        }
        case LayerType::InstanceNormalization:
        {
            auto cLayer = boost::polymorphic_downcast<const InstanceNormalizationLayer*>(&layer);
            const InstanceNormalizationDescriptor& descriptor = cLayer->GetParameters();

            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();

            result = layerSupportObject->IsInstanceNormalizationSupported(
                OverrideDataType(input, dataType),
                OverrideDataType(output, dataType),
                descriptor,
                reason);
            break;
        }
        case LayerType::L2Normalization:
        {
            auto cLayer = boost::polymorphic_downcast<const L2NormalizationLayer*>(&layer);
            const L2NormalizationDescriptor& descriptor = cLayer->GetParameters();

            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();

            result = layerSupportObject->IsL2NormalizationSupported(
                                                OverrideDataType(input, dataType),
                                                OverrideDataType(output, dataType),
                                                descriptor,
                                                reason);
            break;
        }
        case LayerType::Lstm:
        {
            auto cLayer = boost::polymorphic_downcast<const LstmLayer*>(&layer);
            const LstmDescriptor& descriptor = cLayer->GetParameters();

            // All inputs.
            const TensorInfo& input = OverrideDataType(layer.GetInputSlot(0).GetConnection()->GetTensorInfo(),
                                                       dataType);
            const TensorInfo& outputStateIn = OverrideDataType(layer.GetInputSlot(1).GetConnection()->GetTensorInfo(),
                                                               dataType);
            const TensorInfo& cellStateIn = OverrideDataType(layer.GetInputSlot(2).GetConnection()->GetTensorInfo(),
                                                             dataType);
            // All outputs
            const TensorInfo& scratchBuffer = OverrideDataType(layer.GetOutputSlot(0).GetTensorInfo(), dataType);
            const TensorInfo& outputStateOut = OverrideDataType(layer.GetOutputSlot(1).GetTensorInfo(), dataType);
            const TensorInfo& cellStateOut = OverrideDataType(layer.GetOutputSlot(2).GetTensorInfo(), dataType);
            const TensorInfo& output = OverrideDataType(layer.GetOutputSlot(3).GetTensorInfo(), dataType);

            // Basic parameters
            const TensorInfo& inputToForgetWeights
                    = OverrideDataType(cLayer->m_BasicParameters.m_InputToForgetWeights->GetTensorInfo(), dataType);
            const TensorInfo& inputToCellWeights
                    = OverrideDataType(cLayer->m_BasicParameters.m_InputToCellWeights->GetTensorInfo(), dataType);
            const TensorInfo& inputToOutputWeights
                    = OverrideDataType(cLayer->m_BasicParameters.m_InputToOutputWeights->GetTensorInfo(), dataType);
            const TensorInfo& recurrentToForgetWeights
                    = OverrideDataType(cLayer->m_BasicParameters.m_RecurrentToForgetWeights->GetTensorInfo(), dataType);
            const TensorInfo& recurrentToCellWeights
                    = OverrideDataType(cLayer->m_BasicParameters.m_RecurrentToCellWeights->GetTensorInfo(), dataType);
            const TensorInfo& recurrentToOutputWeights
                    = OverrideDataType(cLayer->m_BasicParameters.m_RecurrentToOutputWeights->GetTensorInfo(), dataType);
            const TensorInfo& forgetGateBias
                    = OverrideDataType(cLayer->m_BasicParameters.m_ForgetGateBias->GetTensorInfo(), dataType);
            const TensorInfo& cellBias
                    = OverrideDataType(cLayer->m_BasicParameters.m_CellBias->GetTensorInfo(), dataType);
            const TensorInfo& outputGateBias
                    = OverrideDataType(cLayer->m_BasicParameters.m_OutputGateBias->GetTensorInfo(), dataType);

            LstmInputParamsInfo paramsInfo;

            paramsInfo.m_InputToForgetWeights     = &inputToForgetWeights;
            paramsInfo.m_InputToCellWeights       = &inputToCellWeights;
            paramsInfo.m_InputToOutputWeights     = &inputToOutputWeights;
            paramsInfo.m_RecurrentToForgetWeights = &recurrentToForgetWeights;
            paramsInfo.m_RecurrentToCellWeights   = &recurrentToCellWeights;
            paramsInfo.m_RecurrentToOutputWeights = &recurrentToOutputWeights;
            paramsInfo.m_ForgetGateBias           = &forgetGateBias;
            paramsInfo.m_CellBias                 = &cellBias;
            paramsInfo.m_OutputGateBias           = &outputGateBias;


            // Optional parameters
            TensorInfo optInputToInputWeights;
            TensorInfo optRecurrentToInputWeights;
            TensorInfo optCellToInputWeights;
            TensorInfo optInputGateBias;
            TensorInfo optProjectionWeights;
            TensorInfo optProjectionBias;
            TensorInfo optCellToForgetWeights;
            TensorInfo optCellToOutputWeights;
            TensorInfo optInputLayerNormWeights;
            TensorInfo optForgetLayerNormWeights;
            TensorInfo optCellLayerNormWeights;
            TensorInfo optOutputLayerNormWeights;

            if(!descriptor.m_CifgEnabled)
            {
                optInputToInputWeights =
                    OverrideDataType(cLayer->m_CifgParameters.m_InputToInputWeights->GetTensorInfo(), dataType);
                paramsInfo.m_InputToInputWeights = &optInputToInputWeights;

                optRecurrentToInputWeights =
                    OverrideDataType(cLayer->m_CifgParameters.m_RecurrentToInputWeights->GetTensorInfo(), dataType);
                paramsInfo.m_RecurrentToInputWeights = &optRecurrentToInputWeights;
                if (cLayer->m_CifgParameters.m_CellToInputWeights != nullptr)
                {
                    optCellToInputWeights =
                        OverrideDataType(cLayer->m_CifgParameters.m_CellToInputWeights->GetTensorInfo(), dataType);
                    paramsInfo.m_CellToInputWeights = &optCellToInputWeights;
                }
                optInputGateBias =
                       OverrideDataType(cLayer->m_CifgParameters.m_InputGateBias->GetTensorInfo(), dataType);
                paramsInfo.m_InputGateBias = &optInputGateBias;
            }

            if(descriptor.m_ProjectionEnabled)
            {
                optProjectionWeights =
                    OverrideDataType(cLayer->m_ProjectionParameters.m_ProjectionWeights->GetTensorInfo(), dataType);
                paramsInfo.m_ProjectionWeights = &optProjectionWeights;
                if (cLayer->m_ProjectionParameters.m_ProjectionBias != nullptr)
                {
                    optProjectionBias =
                        OverrideDataType(cLayer->m_ProjectionParameters.m_ProjectionBias->GetTensorInfo(), dataType);
                    paramsInfo.m_ProjectionBias = &optProjectionBias;
                }
            }

            if(descriptor.m_PeepholeEnabled)
            {
                optCellToForgetWeights =
                    OverrideDataType(cLayer->m_PeepholeParameters.m_CellToForgetWeights->GetTensorInfo(), dataType);
                paramsInfo.m_CellToForgetWeights = &optCellToForgetWeights;
                optCellToOutputWeights =
                    OverrideDataType(cLayer->m_PeepholeParameters.m_CellToOutputWeights->GetTensorInfo(), dataType);
                paramsInfo.m_CellToOutputWeights = &optCellToOutputWeights;
            }

            if(descriptor.m_LayerNormEnabled)
            {
                if (!descriptor.m_CifgEnabled)
                {
                    optInputLayerNormWeights = OverrideDataType(
                            cLayer->m_LayerNormParameters.m_InputLayerNormWeights->GetTensorInfo(), dataType);
                    paramsInfo.m_InputLayerNormWeights = &optInputLayerNormWeights;
                }

                optForgetLayerNormWeights = OverrideDataType(
                        cLayer->m_LayerNormParameters.m_ForgetLayerNormWeights->GetTensorInfo(), dataType);
                paramsInfo.m_ForgetLayerNormWeights = &optForgetLayerNormWeights;

                optCellLayerNormWeights = OverrideDataType(
                        cLayer->m_LayerNormParameters.m_CellLayerNormWeights->GetTensorInfo(), dataType);
                paramsInfo.m_CellLayerNormWeights = &optCellLayerNormWeights;

                optOutputLayerNormWeights = OverrideDataType(
                        cLayer->m_LayerNormParameters.m_OutputLayerNormWeights->GetTensorInfo(), dataType);
                paramsInfo.m_OutputLayerNormWeights = &optOutputLayerNormWeights;
            }

            result = layerSupportObject->IsLstmSupported(
                                     input,
                                     outputStateIn,
                                     cellStateIn,
                                     scratchBuffer,
                                     outputStateOut,
                                     cellStateOut,
                                     output,
                                     descriptor,
                                     paramsInfo,
                                     reason);
            break;
        }
        case LayerType::Maximum:
        {
            const TensorInfo& input0 = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& input1 = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();

            result = layerSupportObject->IsMaximumSupported(OverrideDataType(input0, dataType),
                                                            OverrideDataType(input1, dataType),
                                                            OverrideDataType(output, dataType),
                                                            reason);
            break;
        }
        case LayerType::MemCopy:
        {
            const TensorInfo& input  = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();

            result = layerSupportObject->IsMemCopySupported(OverrideDataType(input, dataType),
                                                            OverrideDataType(output, dataType),
                                                            reason);
            break;
        }
        case LayerType::MemImport:
        {
            const TensorInfo& input  = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();

            result = layerSupportObject->IsMemImportSupported(OverrideDataType(input, dataType),
                                                              OverrideDataType(output, dataType),
                                                              reason);
            break;
        }
        case LayerType::Merge:
        {
            const TensorInfo& input0 = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& input1 = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();

            result = layerSupportObject->IsMergeSupported(OverrideDataType(input0, dataType),
                                                          OverrideDataType(input1, dataType),
                                                          OverrideDataType(output, dataType),
                                                          reason);
            break;
        }
        case LayerType::Concat:
        {
            auto cLayer = boost::polymorphic_downcast<const ConcatLayer*>(&layer);

            // Get vector of all inputs.
            auto getTensorInfo = [&dataType](const InputSlot& slot)
                {
                    return OverrideDataType(slot.GetConnectedOutputSlot()->GetTensorInfo(), dataType);
                };
            auto beginI = boost::make_transform_iterator(layer.GetInputSlots().begin(), getTensorInfo);
            auto endI = boost::make_transform_iterator(layer.GetInputSlots().end(), getTensorInfo);
            std::vector<TensorInfo> inputs(beginI, endI);

            auto getTensorInfoPtr = [](const TensorInfo& info)
                {
                    return &info;
                };
            auto beginPtr = boost::make_transform_iterator(inputs.begin(), getTensorInfoPtr);
            auto endPtr = boost::make_transform_iterator(inputs.end(), getTensorInfoPtr);
            std::vector<const TensorInfo*> inputPtrs(beginPtr, endPtr);

            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();

            result = layerSupportObject->IsConcatSupported(inputPtrs, output, cLayer->GetParameters(), reason);


            break;
        }
        case LayerType::Multiplication:
        {
            const TensorInfo& input0 = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& input1 = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsMultiplicationSupported(
                                               OverrideDataType(input0, dataType),
                                               OverrideDataType(input1, dataType),
                                               OverrideDataType(output, dataType),
                                               reason);
            break;
        }
        case LayerType::Normalization:
        {
            auto cLayer = boost::polymorphic_downcast<const NormalizationLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsNormalizationSupported(OverrideDataType(input, dataType),
                                                                  OverrideDataType(output, dataType),
                                                                  cLayer->GetParameters(),
                                                                  reason);
            break;
        }
        case LayerType::Output:
        {
            const TensorInfo& output = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            result = layerSupportObject->IsOutputSupported(OverrideDataType(output, dataType), reason);
            break;
        }
        case LayerType::Permute:
        {
            auto cLayer = boost::polymorphic_downcast<const PermuteLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsPermuteSupported(OverrideDataType(input, dataType),
                                                            OverrideDataType(output, dataType),
                                                            cLayer->GetParameters(),
                                                            reason);
            break;
        }
        case LayerType::Pad:
        {
            auto cLayer = boost::polymorphic_downcast<const PadLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsPadSupported(
                                    OverrideDataType(input, dataType),
                                    OverrideDataType(output, dataType),
                                    cLayer->GetParameters(),
                                    reason);
            break;
        }
        case LayerType::Pooling2d:
        {
            auto cLayer = boost::polymorphic_downcast<const Pooling2dLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsPooling2dSupported(OverrideDataType(input, dataType),
                                                              OverrideDataType(output, dataType),
                                                              cLayer->GetParameters(),
                                                              reason);
            break;
        }
        case LayerType::PreCompiled:
        {
            auto cLayer = boost::polymorphic_downcast<const PreCompiledLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            result = layerSupportObject->IsPreCompiledSupported(OverrideDataType(input, dataType),
                                                                cLayer->GetParameters(),
                                                                reason);
            break;
        }
        case LayerType::Quantize:
        {
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsQuantizeSupported(input, output, reason);
            break;
        }
        case LayerType::QuantizedLstm:
        {
            auto cLayer = boost::polymorphic_downcast<const QuantizedLstmLayer*>(&layer);

            // Inputs
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& previousCellStateIn = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            const TensorInfo& previousOutputIn = layer.GetInputSlot(2).GetConnection()->GetTensorInfo();

            // Outputs
            const TensorInfo& cellStateOut = layer.GetOutputSlot(0).GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(1).GetTensorInfo();

            // QuantizedLstm parameters
            QuantizedLstmInputParamsInfo paramsInfo;

            paramsInfo.m_InputToInputWeights      =
                    &cLayer->m_QuantizedLstmParameters.m_InputToInputWeights->GetTensorInfo();
            paramsInfo.m_InputToForgetWeights     =
                    &cLayer->m_QuantizedLstmParameters.m_InputToForgetWeights->GetTensorInfo();
            paramsInfo.m_InputToCellWeights       =
                    &cLayer->m_QuantizedLstmParameters.m_InputToCellWeights->GetTensorInfo();
            paramsInfo.m_InputToOutputWeights     =
                    &cLayer->m_QuantizedLstmParameters.m_InputToOutputWeights->GetTensorInfo();

            paramsInfo.m_RecurrentToInputWeights  =
                    &cLayer->m_QuantizedLstmParameters.m_RecurrentToInputWeights->GetTensorInfo();
            paramsInfo.m_RecurrentToForgetWeights =
                    &cLayer->m_QuantizedLstmParameters.m_RecurrentToForgetWeights->GetTensorInfo();
            paramsInfo.m_RecurrentToCellWeights   =
                    &cLayer->m_QuantizedLstmParameters.m_RecurrentToCellWeights->GetTensorInfo();
            paramsInfo.m_RecurrentToOutputWeights =
                    &cLayer->m_QuantizedLstmParameters.m_RecurrentToOutputWeights->GetTensorInfo();

            paramsInfo.m_InputGateBias            =
                    &cLayer->m_QuantizedLstmParameters.m_InputGateBias->GetTensorInfo();
            paramsInfo.m_ForgetGateBias           =
                    &cLayer->m_QuantizedLstmParameters.m_ForgetGateBias->GetTensorInfo();
            paramsInfo.m_CellBias                 =
                    &cLayer->m_QuantizedLstmParameters.m_CellBias->GetTensorInfo();
            paramsInfo.m_OutputGateBias           =
                    &cLayer->m_QuantizedLstmParameters.m_OutputGateBias->GetTensorInfo();;

            result = layerSupportObject->IsQuantizedLstmSupported(input,
                                                                  previousCellStateIn,
                                                                  previousOutputIn,
                                                                  cellStateOut,
                                                                  output,
                                                                  paramsInfo,
                                                                  reason);
            break;
        }
        case LayerType::Division:
        {
            const TensorInfo& input0 = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& input1 = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsDivisionSupported(
                                         OverrideDataType(input0, dataType),
                                         OverrideDataType(input1, dataType),
                                         OverrideDataType(output, dataType),
                                         reason);
            break;
        }
        case LayerType::Reshape:
        {
            auto cLayer = boost::polymorphic_downcast<const ReshapeLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            result = layerSupportObject->IsReshapeSupported(OverrideDataType(input, dataType),
                                                            cLayer->GetParameters(),
                                                            reason);
            break;
        }
        case LayerType::Resize:
        {
            auto cLayer = boost::polymorphic_downcast<const ResizeLayer*>(&layer);
            const TensorInfo& input  = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsResizeSupported(OverrideDataType(input, dataType),
                                                           OverrideDataType(output, dataType),
                                                           cLayer->GetParameters(),
                                                           reason);
            break;
        }
        case LayerType::Rsqrt:
        {
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsRsqrtSupported(OverrideDataType(input, dataType),
                                                          OverrideDataType(output, dataType),
                                                          reason);
            break;
        }
        case LayerType::Slice:
        {
            auto cLayer = boost::polymorphic_downcast<const SliceLayer*>(&layer);

            const TensorInfo& input  = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();

            result = layerSupportObject->IsSliceSupported(OverrideDataType(input, dataType),
                                                          OverrideDataType(output, dataType),
                                                          cLayer->GetParameters(),
                                                          reason);
            break;
        }
        case LayerType::Softmax:
        {
            auto cLayer = boost::polymorphic_downcast<const SoftmaxLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsSoftmaxSupported(OverrideDataType(input, dataType),
                                                            OverrideDataType(output, dataType),
                                                            cLayer->GetParameters(),
                                                            reason);
            break;
        }
        case LayerType::SpaceToBatchNd:
        {
            auto cLayer = boost::polymorphic_downcast<const SpaceToBatchNdLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsSpaceToBatchNdSupported(OverrideDataType(input, dataType),
                                                                   OverrideDataType(output, dataType),
                                                                   cLayer->GetParameters(),
                                                                   reason);
            break;
        }
        case LayerType::SpaceToDepth:
        {
            auto cLayer = boost::polymorphic_downcast<const SpaceToDepthLayer*>(&layer);

            const TensorInfo& input  = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();

            result = layerSupportObject->IsSpaceToDepthSupported(OverrideDataType(input, dataType),
                                                                 OverrideDataType(output, dataType),
                                                                 cLayer->GetParameters(),
                                                                 reason);
            break;
        }
        case LayerType::Splitter:
        {
            auto cLayer = boost::polymorphic_downcast<const SplitterLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();

            // Get vector of all outputs.
            auto getTensorInfo = [&dataType](const OutputSlot& slot)
            {
                return OverrideDataType(slot.GetTensorInfo(), dataType);
            };
            auto beginI = boost::make_transform_iterator(layer.GetOutputSlots().begin(), getTensorInfo);
            auto endI = boost::make_transform_iterator(layer.GetOutputSlots().end(), getTensorInfo);
            std::vector<TensorInfo> outputs(beginI, endI);

            const std::vector<std::reference_wrapper<TensorInfo>> outputPtrs(outputs.begin(), outputs.end());

            result = layerSupportObject->IsSplitterSupported(OverrideDataType(input, dataType),
                                                             outputPtrs,
                                                             cLayer->GetParameters(),
                                                             reason);
            break;
        }
        case LayerType::Stack:
        {
            auto cLayer = boost::polymorphic_downcast<const StackLayer*>(&layer);

            // Get vector of all inputs.
            auto getTensorInfo = [&dataType](const InputSlot& slot)
                {
                    return OverrideDataType(slot.GetConnectedOutputSlot()->GetTensorInfo(), dataType);
                };
            auto beginI = boost::make_transform_iterator(layer.GetInputSlots().begin(), getTensorInfo);
            auto endI = boost::make_transform_iterator(layer.GetInputSlots().end(), getTensorInfo);
            std::vector<TensorInfo> inputs(beginI, endI);

            auto getTensorInfoPtr = [](const TensorInfo& info)
                {
                    return &info;
                };
            auto beginPtr = boost::make_transform_iterator(inputs.begin(), getTensorInfoPtr);
            auto endPtr = boost::make_transform_iterator(inputs.end(), getTensorInfoPtr);
            std::vector<const TensorInfo*> inputPtrs(beginPtr, endPtr);

            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();

            result = layerSupportObject->IsStackSupported(inputPtrs, output, cLayer->GetParameters(), reason);

            break;
        }
        case LayerType::StridedSlice:
        {
            auto cLayer = boost::polymorphic_downcast<const StridedSliceLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsStridedSliceSupported(OverrideDataType(input, dataType),
                                                                 OverrideDataType(output, dataType),
                                                                 cLayer->GetParameters(),
                                                                 reason);
            break;
        }
        case LayerType::Subtraction:
        {
            const TensorInfo& input0 = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& input1 = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsSubtractionSupported(
                                            OverrideDataType(input0, dataType),
                                            OverrideDataType(input1, dataType),
                                            OverrideDataType(output, dataType),
                                            reason);
            break;
        }
        case LayerType::Switch:
        {
            const TensorInfo& input0 = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& input1 = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            const TensorInfo& output0 = layer.GetOutputSlot(0).GetTensorInfo();
            const TensorInfo& output1 = layer.GetOutputSlot(1).GetTensorInfo();
            result = layerSupportObject->IsSwitchSupported(OverrideDataType(input0, dataType),
                                                           OverrideDataType(input1, dataType),
                                                           OverrideDataType(output0, dataType),
                                                           OverrideDataType(output1, dataType),
                                                           reason);
            break;
        }
        case LayerType::Mean:
        {
            auto cLayer = boost::polymorphic_downcast<const MeanLayer*>(&layer);
            const TensorInfo& input = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsMeanSupported(
                                     OverrideDataType(input, dataType),
                                     OverrideDataType(output, dataType),
                                     cLayer->GetParameters(),
                                     reason);
            break;
        }
        case LayerType::Minimum:
        {
            const TensorInfo& input0 = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& input1 = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsMinimumSupported(OverrideDataType(input0, dataType),
                                                            OverrideDataType(input1, dataType),
                                                            OverrideDataType(output, dataType),
                                                            reason);
            break;
        }
        case LayerType::Greater:
        {
            const TensorInfo& input0 = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& input1 = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsGreaterSupported(OverrideDataType(input0, dataType),
                                                            OverrideDataType(input1, dataType),
                                                            OverrideDataType(output, DataType::Boolean),
                                                            reason);
            break;
        }
        case LayerType::Prelu:
        {
            const TensorInfo& input  = layer.GetInputSlot(0).GetConnection()->GetTensorInfo();
            const TensorInfo& alpha  = layer.GetInputSlot(1).GetConnection()->GetTensorInfo();
            const TensorInfo& output = layer.GetOutputSlot(0).GetTensorInfo();
            result = layerSupportObject->IsPreluSupported(OverrideDataType(input,  dataType),
                                                          OverrideDataType(alpha,  dataType),
                                                          OverrideDataType(output, dataType),
                                                          reason);
            break;
        }
        case LayerType::TransposeConvolution2d:
        {
            auto cLayer = boost::polymorphic_downcast<const TransposeConvolution2dLayer*>(&layer);

            const TensorInfo input  = OverrideDataType(layer.GetInputSlot(0).GetConnection()->GetTensorInfo(),
                                                       dataType);
            const TensorInfo output = OverrideDataType(layer.GetOutputSlot(0).GetTensorInfo(), dataType);

            const TransposeConvolution2dDescriptor& descriptor  = cLayer->GetParameters();

            Optional<TensorInfo> biases;
            if (descriptor.m_BiasEnabled)
            {
                BOOST_ASSERT(cLayer->m_Bias.get() != nullptr);
                biases = OverrideDataType(cLayer->m_Bias->GetTensorInfo(),
                                          GetBiasTypeFromWeightsType(dataType));
            }

            BOOST_ASSERT(cLayer->m_Weight.get() != nullptr);
            const TensorInfo weights = OverrideDataType(cLayer->m_Weight->GetTensorInfo(), dataType);

            result = layerSupportObject->IsTransposeConvolution2dSupported(input,
                                                                           output,
                                                                           descriptor,
                                                                           weights,
                                                                           biases,
                                                                           reason);

            break;
        }
        default:
        {
            BOOST_ASSERT_MSG(false, "WorkloadFactory did not recognise type of layer.");
            reason.value() = "Unrecognised layer type";
            result = false;
            break;
        }
    }
    return result;
}

bool IWorkloadFactory::IsLayerSupported(const IConnectableLayer& connectableLayer,
                                        Optional<DataType> dataType,
                                        std::string& outReasonIfUnsupported)
{
    auto layer = boost::polymorphic_downcast<const Layer*>(&connectableLayer);
    return IsLayerSupported(layer->GetBackendId(), connectableLayer, dataType, outReasonIfUnsupported);
}

// Default Implementations
std::unique_ptr<IWorkload> IWorkloadFactory::CreateAbs(const AbsQueueDescriptor& descriptor,
                                                       const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateActivation(const ActivationQueueDescriptor& descriptor,
                                                              const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateAddition(const AdditionQueueDescriptor& descriptor,
                                                            const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateArgMinMax(const ArgMinMaxQueueDescriptor& descriptor,
                                                             const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateBatchNormalization(
    const BatchNormalizationQueueDescriptor& descriptor, const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateBatchToSpaceNd(const BatchToSpaceNdQueueDescriptor& descriptor,
                                                                  const WorkloadInfo& Info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateConcat(const ConcatQueueDescriptor& descriptor,
                                                          const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateConstant(const ConstantQueueDescriptor& descriptor,
                                                            const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateConvertFp16ToFp32(const ConvertFp16ToFp32QueueDescriptor& descriptor,
                                                                     const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateConvertFp32ToFp16(const ConvertFp32ToFp16QueueDescriptor& descriptor,
                                                                     const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateConvolution2d(const Convolution2dQueueDescriptor& descriptor,
                                                                 const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateDebug(const DebugQueueDescriptor& descriptor,
                                                         const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateDepthToSpace(const DepthToSpaceQueueDescriptor& descriptor,
                                                                const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateDepthwiseConvolution2d(
    const DepthwiseConvolution2dQueueDescriptor& descriptor, const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateDequantize(
    const DequantizeQueueDescriptor& descriptor, const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateDetectionPostProcess(
    const DetectionPostProcessQueueDescriptor& descriptor, const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateDivision(const DivisionQueueDescriptor& descriptor,
                                                            const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateEqual(const EqualQueueDescriptor& descriptor,
                                                         const WorkloadInfo& Info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateFakeQuantization(const FakeQuantizationQueueDescriptor& descriptor,
                                                                    const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateFloor(const FloorQueueDescriptor& descriptor,
                                                         const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateFullyConnected(const FullyConnectedQueueDescriptor& descriptor,
                                                                  const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateGather(const GatherQueueDescriptor& descriptor,
                                                          const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateGreater(const GreaterQueueDescriptor& descriptor,
                                                           const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateInstanceNormalization(
    const InstanceNormalizationQueueDescriptor& descriptor,
    const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateL2Normalization(const L2NormalizationQueueDescriptor& descriptor,
                                                                   const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateLstm(const LstmQueueDescriptor& descriptor,
                                                        const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateMaximum(const MaximumQueueDescriptor& descriptor,
                                                           const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateMean(const MeanQueueDescriptor& descriptor,
                                                        const WorkloadInfo& Info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateMemCopy(const MemCopyQueueDescriptor& descriptor,
                                                           const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateMemImport(const MemImportQueueDescriptor& descriptor,
                                                             const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateMerge(const MergeQueueDescriptor& descriptor,
                                                         const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateMerger(const MergerQueueDescriptor& descriptor,
                                                          const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateMinimum(const MinimumQueueDescriptor& descriptor,
                                                           const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateMultiplication(const MultiplicationQueueDescriptor& descriptor,
                                                                  const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateNormalization(const NormalizationQueueDescriptor& descriptor,
                                                                 const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateOutput(const OutputQueueDescriptor& descriptor,
                                                          const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreatePad(const PadQueueDescriptor& descriptor,
                                                       const WorkloadInfo& Info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreatePermute(const PermuteQueueDescriptor& descriptor,
                                                           const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreatePooling2d(const Pooling2dQueueDescriptor& descriptor,
                                                             const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreatePreCompiled(const PreCompiledQueueDescriptor& descriptor,
                                                               const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreatePrelu(const PreluQueueDescriptor &descriptor,
                                                         const WorkloadInfo &info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateQuantize(const QuantizeQueueDescriptor& descriptor,
                                                            const WorkloadInfo& Info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateQuantizedLstm(const QuantizedLstmQueueDescriptor& descriptor,
                                                                 const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateReshape(const ReshapeQueueDescriptor& descriptor,
                                                           const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateResizeBilinear(const ResizeBilinearQueueDescriptor& descriptor,
                                                                  const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateResize(const ResizeQueueDescriptor& descriptor,
                                                            const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateRsqrt(const RsqrtQueueDescriptor& descriptor,
                                                         const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateSlice(const SliceQueueDescriptor& descriptor,
                                                         const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateSoftmax(const SoftmaxQueueDescriptor& descriptor,
                                                           const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateSplitter(const SplitterQueueDescriptor& descriptor,
                                                            const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateSpaceToBatchNd(const SpaceToBatchNdQueueDescriptor& descriptor,
                                                                  const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateSpaceToDepth(const SpaceToDepthQueueDescriptor& descriptor,
                                                                const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateStack(const StackQueueDescriptor& descriptor,
                                                         const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateStridedSlice(const StridedSliceQueueDescriptor& descriptor,
                                                                const WorkloadInfo& Info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateSubtraction(const SubtractionQueueDescriptor& descriptor,
                                                               const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateSwitch(const SwitchQueueDescriptor& descriptor,
                                                          const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

std::unique_ptr<IWorkload> IWorkloadFactory::CreateTransposeConvolution2d(
    const TransposeConvolution2dQueueDescriptor& descriptor,
    const WorkloadInfo& info) const
{
    return std::unique_ptr<IWorkload>();
}

} // namepsace armnn
