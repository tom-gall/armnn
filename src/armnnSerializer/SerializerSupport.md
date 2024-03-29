# The layers that ArmNN SDK Serializer currently supports.

This reference guide provides a list of layers which can be serialized currently by the Arm NN SDK.

## Fully supported

The Arm NN SDK Serializer currently supports the following layers:

* Abs
* Activation
* Addition
* ArgMinMax
* BatchToSpaceNd
* BatchNormalization
* Concat
* Constant
* Convolution2d
* DepthToSpace
* DepthwiseConvolution2d
* Dequantize
* DetectionPostProcess
* Division
* Equal
* Floor
* FullyConnected
* Gather
* Greater
* Input
* InstanceNormalization
* L2Normalization
* Lstm
* Maximum
* Mean
* Merge
* Minimum
* Multiplication
* Normalization
* Output
* Pad
* Permute
* Pooling2d
* Prelu
* Quantize
* QuantizedLstm
* Reshape
* Resize
* ResizeBilinear
* Rsqrt
* Slice
* Softmax
* SpaceToBatchNd
* SpaceToDepth
* Splitter
* Stack
* StridedSlice
* Subtraction
* Switch
* TransposeConvolution2d

More machine learning layers will be supported in future releases.

Note: Merger layer has been renamed Concat. Serializations of the old
      format with Merger layers will deserialize to Concat layers to
      maintain backward compatibility.
