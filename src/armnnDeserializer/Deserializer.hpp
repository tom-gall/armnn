//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "armnn/INetwork.hpp"
#include "armnnDeserializer/IDeserializer.hpp"
#include <ArmnnSchema_generated.h>

#include <unordered_map>

namespace armnnDeserializer
{
class Deserializer : public IDeserializer
{
public:
    // Shorthands for deserializer types
    using ConstTensorRawPtr = const armnnSerializer::ConstTensor *;
    using GraphPtr = const armnnSerializer::SerializedGraph *;
    using TensorRawPtr = const armnnSerializer::TensorInfo *;
    using PoolingDescriptor = const armnnSerializer::Pooling2dDescriptor *;
    using NormalizationDescriptorPtr = const armnnSerializer::NormalizationDescriptor *;
    using LstmDescriptorPtr = const armnnSerializer::LstmDescriptor *;
    using LstmInputParamsPtr = const armnnSerializer::LstmInputParams *;
    using QunatizedLstmInputParamsPtr = const armnnSerializer::QuantizedLstmInputParams *;
    using TensorRawPtrVector = std::vector<TensorRawPtr>;
    using LayerRawPtr = const armnnSerializer::LayerBase *;
    using LayerBaseRawPtr = const armnnSerializer::LayerBase *;
    using LayerBaseRawPtrVector = std::vector<LayerBaseRawPtr>;

public:

    /// Create an input network from binary file contents
    armnn::INetworkPtr CreateNetworkFromBinary(const std::vector<uint8_t>& binaryContent) override;

    /// Create an input network from a binary input stream
    armnn::INetworkPtr CreateNetworkFromBinary(std::istream& binaryContent) override;

    /// Retrieve binding info (layer id and tensor info) for the network input identified by the given layer name
    BindingPointInfo GetNetworkInputBindingInfo(unsigned int layerId, const std::string& name) const override;

    /// Retrieve binding info (layer id and tensor info) for the network output identified by the given layer name
    BindingPointInfo GetNetworkOutputBindingInfo(unsigned int layerId, const std::string& name) const override;

    Deserializer();
    ~Deserializer() {}

public:
    // testable helpers
    static GraphPtr LoadGraphFromBinary(const uint8_t* binaryContent, size_t len);
    static TensorRawPtrVector GetInputs(const GraphPtr& graph, unsigned int layerIndex);
    static TensorRawPtrVector GetOutputs(const GraphPtr& graph, unsigned int layerIndex);
    static LayerBaseRawPtr GetBaseLayer(const GraphPtr& graphPtr, unsigned int layerIndex);
    static int32_t GetBindingLayerInfo(const GraphPtr& graphPtr, unsigned int layerIndex);
    static std::string GetLayerName(const GraphPtr& graph, unsigned int index);
    static armnn::Pooling2dDescriptor GetPoolingDescriptor(PoolingDescriptor pooling2dDescriptor,
                                                           unsigned int layerIndex);
    static armnn::NormalizationDescriptor GetNormalizationDescriptor(
        NormalizationDescriptorPtr normalizationDescriptor, unsigned int layerIndex);
    static armnn::LstmDescriptor GetLstmDescriptor(LstmDescriptorPtr lstmDescriptor);
    static armnn::LstmInputParams GetLstmInputParams(LstmDescriptorPtr lstmDescriptor,
                                                     LstmInputParamsPtr lstmInputParams);
    static armnn::TensorInfo OutputShapeOfReshape(const armnn::TensorInfo & inputTensorInfo,
                                                  const std::vector<uint32_t> & targetDimsIn);

private:
    // No copying allowed until it is wanted and properly implemented
    Deserializer(const Deserializer&) = delete;
    Deserializer& operator=(const Deserializer&) = delete;

    /// Create the network from an already loaded flatbuffers graph
    armnn::INetworkPtr CreateNetworkFromGraph(GraphPtr graph);

    // signature for the parser functions
    using LayerParsingFunction = void(Deserializer::*)(GraphPtr graph, unsigned int layerIndex);

    void ParseUnsupportedLayer(GraphPtr graph, unsigned int layerIndex);
    void ParseAbs(GraphPtr graph, unsigned int layerIndex);
    void ParseActivation(GraphPtr graph, unsigned int layerIndex);
    void ParseAdd(GraphPtr graph, unsigned int layerIndex);
    void ParseArgMinMax(GraphPtr graph, unsigned int layerIndex);
    void ParseBatchToSpaceNd(GraphPtr graph, unsigned int layerIndex);
    void ParseBatchNormalization(GraphPtr graph, unsigned int layerIndex);
    void ParseConcat(GraphPtr graph, unsigned int layerIndex);
    void ParseConstant(GraphPtr graph, unsigned int layerIndex);
    void ParseConvolution2d(GraphPtr graph, unsigned int layerIndex);
    void ParseDepthToSpace(GraphPtr graph, unsigned int layerIndex);
    void ParseDepthwiseConvolution2d(GraphPtr graph, unsigned int layerIndex);
    void ParseDequantize(GraphPtr graph, unsigned int layerIndex);
    void ParseDetectionPostProcess(GraphPtr graph, unsigned int layerIndex);
    void ParseDivision(GraphPtr graph, unsigned int layerIndex);
    void ParseEqual(GraphPtr graph, unsigned int layerIndex);
    void ParseFloor(GraphPtr graph, unsigned int layerIndex);
    void ParseFullyConnected(GraphPtr graph, unsigned int layerIndex);
    void ParseGather(GraphPtr graph, unsigned int layerIndex);
    void ParseGreater(GraphPtr graph, unsigned int layerIndex);
    void ParseInstanceNormalization(GraphPtr graph, unsigned int layerIndex);
    void ParseL2Normalization(GraphPtr graph, unsigned int layerIndex);
    void ParseMaximum(GraphPtr graph, unsigned int layerIndex);
    void ParseMean(GraphPtr graph, unsigned int layerIndex);
    void ParseMinimum(GraphPtr graph, unsigned int layerIndex);
    void ParseMerge(GraphPtr graph, unsigned int layerIndex);
    void ParseMultiplication(GraphPtr graph, unsigned int layerIndex);
    void ParseNormalization(GraphPtr graph, unsigned int layerIndex);
    void ParseLstm(GraphPtr graph, unsigned int layerIndex);
    void ParseQuantizedLstm(GraphPtr graph, unsigned int layerIndex);
    void ParsePad(GraphPtr graph, unsigned int layerIndex);
    void ParsePermute(GraphPtr graph, unsigned int layerIndex);
    void ParsePooling2d(GraphPtr graph, unsigned int layerIndex);
    void ParsePrelu(GraphPtr graph, unsigned int layerIndex);
    void ParseQuantize(GraphPtr graph, unsigned int layerIndex);
    void ParseReshape(GraphPtr graph, unsigned int layerIndex);
    void ParseResize(GraphPtr graph, unsigned int layerIndex);
    void ParseResizeBilinear(GraphPtr graph, unsigned int layerIndex);
    void ParseRsqrt(GraphPtr graph, unsigned int layerIndex);
    void ParseSlice(GraphPtr graph, unsigned int layerIndex);
    void ParseSoftmax(GraphPtr graph, unsigned int layerIndex);
    void ParseSpaceToBatchNd(GraphPtr graph, unsigned int layerIndex);
    void ParseSpaceToDepth(GraphPtr graph, unsigned int layerIndex);
    void ParseSplitter(GraphPtr graph, unsigned int layerIndex);
    void ParseStack(GraphPtr graph, unsigned int layerIndex);
    void ParseStridedSlice(GraphPtr graph, unsigned int layerIndex);
    void ParseSubtraction(GraphPtr graph, unsigned int layerIndex);
    void ParseSwitch(GraphPtr graph, unsigned int layerIndex);
    void ParseTransposeConvolution2d(GraphPtr graph, unsigned int layerIndex);

    void RegisterInputSlots(GraphPtr graph, uint32_t layerIndex,
                            armnn::IConnectableLayer* layer);
    void RegisterOutputSlots(GraphPtr graph, uint32_t layerIndex,
                             armnn::IConnectableLayer* layer);

    // NOTE index here must be from flatbuffer object index property
    void RegisterOutputSlotOfConnection(uint32_t sourceLayerIndex, uint32_t outputSlotIndex, armnn::IOutputSlot* slot);
    void RegisterInputSlotOfConnection(uint32_t sourceLayerIndex, uint32_t outputSlotIndex, armnn::IInputSlot* slot);

    void ResetParser();

    void SetupInputLayers(GraphPtr graphPtr);
    void SetupOutputLayers(GraphPtr graphPtr);

    /// Helper to get the index of the layer in the flatbuffer vector from its index property
    unsigned int GetLayerIndexInVector(GraphPtr graph, unsigned int index);

    /// The network we're building. Gets cleared after it is passed to the user
    armnn::INetworkPtr                    m_Network;
    std::vector<LayerParsingFunction>     m_ParserFunctions;

    using NameToBindingInfo = std::pair<std::string, BindingPointInfo >;
    std::vector<NameToBindingInfo>    m_InputBindings;
    std::vector<NameToBindingInfo>    m_OutputBindings;

    /// This struct describe connections for each layer
    struct Connections
    {
        // Maps output slot index (property in flatbuffer object) to IOutputSlot pointer
        std::unordered_map<unsigned int, armnn::IOutputSlot*> outputSlots;

        // Maps output slot index to IInputSlot pointer the output slot should be connected to
        std::unordered_map<unsigned int, std::vector<armnn::IInputSlot*>> inputSlots;
    };

    /// Maps layer index (index property in flatbuffer object) to Connections for each layer
    std::unordered_map<unsigned int, Connections> m_GraphConnections;
};

} //namespace armnnDeserializer
