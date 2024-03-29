//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "TensorUtils.hpp"
#include <backendsCommon/ITensorHandle.hpp>

#include <boost/assert.hpp>
#include <boost/format.hpp>
#include <boost/numeric/conversion/cast.hpp>

namespace armnnUtils
{

armnn::TensorShape GetTensorShape(unsigned int numberOfBatches,
                                  unsigned int numberOfChannels,
                                  unsigned int height,
                                  unsigned int width,
                                  const armnn::DataLayout dataLayout)
{
    switch (dataLayout)
    {
        case armnn::DataLayout::NCHW:
            return armnn::TensorShape({numberOfBatches, numberOfChannels, height, width});
        case armnn::DataLayout::NHWC:
            return armnn::TensorShape({numberOfBatches, height, width, numberOfChannels});
        default:
            throw armnn::InvalidArgumentException("Unknown data layout ["
                                                  + std::to_string(static_cast<int>(dataLayout)) +
                                                  "]", CHECK_LOCATION());
    }
}

armnn::TensorInfo GetTensorInfo(unsigned int numberOfBatches,
                                unsigned int numberOfChannels,
                                unsigned int height,
                                unsigned int width,
                                const armnn::DataLayout dataLayout,
                                const armnn::DataType dataType)
{
    switch (dataLayout)
    {
        case armnn::DataLayout::NCHW:
            return armnn::TensorInfo({numberOfBatches, numberOfChannels, height, width}, dataType);
        case armnn::DataLayout::NHWC:
            return armnn::TensorInfo({numberOfBatches, height, width, numberOfChannels}, dataType);
        default:
            throw armnn::InvalidArgumentException("Unknown data layout ["
                                                  + std::to_string(static_cast<int>(dataLayout)) +
                                                  "]", CHECK_LOCATION());
    }
}

std::pair<float, float> FindMinMax(armnn::ITensorHandle* tensorHandle)
{
    auto tensor_data = static_cast<const float *>(tensorHandle->Map(true));
    auto tensor_size = tensorHandle->GetShape().GetNumElements();

    // Set min/max initially to first value in tensor
    float min = tensor_data[0];
    float max = tensor_data[0];

    // Loop over rest of tensor and update min/max if necessary
    for (unsigned int val = 1; val < tensor_size; val++)
    {
        if (tensor_data[val] < min)
        {
            min = tensor_data[val];
        }
        else if (tensor_data[val] > max)
        {
            max = tensor_data[val];
        }
    }

    tensorHandle->Unmap();

    return std::make_pair(min, max);
}

armnn::TensorShape ExpandDims(const armnn::TensorShape& tensorShape, int axis)
{
    unsigned int outputDim = tensorShape.GetNumDimensions() + 1;

    if (axis < -boost::numeric_cast<int>(outputDim) || axis > boost::numeric_cast<int>(tensorShape.GetNumDimensions()))
    {
        throw armnn::InvalidArgumentException(
            boost::str(boost::format("Invalid expansion axis %1% for %2%D input tensor. %3%") %
                       axis %
                       tensorShape.GetNumDimensions() %
                       CHECK_LOCATION().AsString()));
    }

    if (axis < 0)
    {
        axis = boost::numeric_cast<int>(outputDim) + axis;
    }

    std::vector<unsigned int> outputShape;
    for (unsigned int i = 0; i < tensorShape.GetNumDimensions(); ++i)
    {
        outputShape.push_back(tensorShape[i]);
    }
    outputShape.insert(outputShape.begin() + axis, 1);

    return armnn::TensorShape(outputDim, outputShape.data());
}

unsigned int GetNumElementsBetween(const armnn::TensorShape& shape,
                                   const unsigned int firstAxisInclusive,
                                   const unsigned int lastAxisExclusive)
{
    BOOST_ASSERT(0 <= firstAxisInclusive);
    BOOST_ASSERT(firstAxisInclusive <= lastAxisExclusive);
    BOOST_ASSERT(lastAxisExclusive <= shape.GetNumDimensions());
    unsigned int count = 1;
    for (unsigned int i = firstAxisInclusive; i < lastAxisExclusive; i++)
    {
        count *= shape[i];
    }
    return count;
}

unsigned int GetUnsignedAxis(const unsigned int inputDimension, const int axis)
{
    BOOST_ASSERT_MSG(axis < boost::numeric_cast<int>(inputDimension),
                     "Required axis index greater than number of dimensions.");
    BOOST_ASSERT_MSG(axis >= -boost::numeric_cast<int>(inputDimension),
                     "Required axis index lower than negative of the number of dimensions");

    unsigned int uAxis = axis < 0  ?
                         inputDimension - boost::numeric_cast<unsigned int>(abs(axis))
                         : boost::numeric_cast<unsigned int>(axis);
    return uAxis;
}

}
