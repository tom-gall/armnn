#
# Copyright © 2017 ARM Ltd. All rights reserved.
# SPDX-License-Identifier: MIT
#

# COMMON_SOURCES contains the list of files to be included
# in the Android build and it is picked up by the Android.mk
# file in the root of ArmNN

COMMON_SOURCES := \
    BackendRegistry.cpp \
    CpuTensorHandle.cpp \
    DynamicBackend.cpp \
    DynamicBackendUtils.cpp \
    IBackendInternal.cpp \
    ITensorHandleFactory.cpp \
    LayerSupportBase.cpp \
    MemCopyWorkload.cpp \
    MemImportWorkload.cpp \
    MemSyncWorkload.cpp \
    OptimizationViews.cpp \
    OutputHandler.cpp \
    TensorHandleFactoryRegistry.cpp \
    WorkloadData.cpp \
    WorkloadFactory.cpp \
    WorkloadUtils.cpp

# COMMON_TEST_SOURCES contains the list of files to be included
# in the Android unit test build (armnn-tests) and it is picked
# up by the Android.mk file in the root of ArmNN

COMMON_TEST_SOURCES := \
    test/CommonTestUtils.cpp \
    test/JsonPrinterTestImpl.cpp \
    test/QuantizedLstmEndToEndTestImpl.cpp \
    test/SpaceToDepthEndToEndTestImpl.cpp \
    test/TensorCopyUtils.cpp \
    test/layerTests/AbsTestImpl.cpp \
    test/layerTests/ActivationTestImpl.cpp \
    test/layerTests/AdditionTestImpl.cpp \
    test/layerTests/ArgMinMaxTestImpl.cpp \
    test/layerTests/BatchNormalizationTestImpl.cpp \
    test/layerTests/ConcatTestImpl.cpp \
    test/layerTests/ConstantTestImpl.cpp \
    test/layerTests/Conv2dTestImpl.cpp \
    test/layerTests/ConvertFp16ToFp32TestImpl.cpp \
    test/layerTests/ConvertFp32ToFp16TestImpl.cpp \
    test/layerTests/DebugTestImpl.cpp \
    test/layerTests/DepthToSpaceTestImpl.cpp \
    test/layerTests/DequantizeTestImpl.cpp \
    test/layerTests/DivisionTestImpl.cpp \
    test/layerTests/EqualTestImpl.cpp \
    test/layerTests/FakeQuantizationTestImpl.cpp \
    test/layerTests/FloorTestImpl.cpp \
    test/layerTests/FullyConnectedTestImpl.cpp \
    test/layerTests/GatherTestImpl.cpp \
    test/layerTests/GreaterTestImpl.cpp \
    test/layerTests/InstanceNormalizationTestImpl.cpp \
    test/layerTests/L2NormalizationTestImpl.cpp \
    test/layerTests/LstmTestImpl.cpp \
    test/layerTests/MaximumTestImpl.cpp \
    test/layerTests/MinimumTestImpl.cpp \
    test/layerTests/MultiplicationTestImpl.cpp \
    test/layerTests/NormalizationTestImpl.cpp \
    test/layerTests/PadTestImpl.cpp \
    test/layerTests/Pooling2dTestImpl.cpp \
    test/layerTests/ReshapeTestImpl.cpp \
    test/layerTests/RsqrtTestImpl.cpp \
    test/layerTests/SliceTestImpl.cpp \
    test/layerTests/QuantizeTestImpl.cpp \
    test/layerTests/SoftmaxTestImpl.cpp \
    test/layerTests/SpaceToBatchNdTestImpl.cpp \
    test/layerTests/SpaceToDepthTestImpl.cpp \
    test/layerTests/SplitterTestImpl.cpp \
    test/layerTests/StackTestImpl.cpp \
    test/layerTests/StridedSliceTestImpl.cpp \
    test/layerTests/SubtractionTestImpl.cpp

ifeq ($(ARMNN_REF_ENABLED),1)
COMMON_TEST_SOURCES += \
    test/WorkloadDataValidation.cpp
endif # ARMNN_REF_ENABLED == 1
