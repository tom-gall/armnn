#
# Copyright © 2017 Arm Ltd. All rights reserved.
# SPDX-License-Identifier: MIT
#

list(APPEND armnnRefBackendUnitTests_sources
    ArgMinMaxTests.cpp
    RefCreateWorkloadTests.cpp
    RefDetectionPostProcessTests.cpp
    RefEndToEndTests.cpp
    RefJsonPrinterTests.cpp
    RefLayerSupportTests.cpp
    RefLayerTests.cpp
    RefMemoryManagerTests.cpp
    RefOptimizedNetworkTests.cpp
    RefRuntimeTests.cpp
    RefTensorHandleTests.cpp
    RefWorkloadFactoryHelper.hpp
)

add_library(armnnRefBackendUnitTests OBJECT ${armnnRefBackendUnitTests_sources})
target_include_directories(armnnRefBackendUnitTests PRIVATE ${PROJECT_SOURCE_DIR}/src/armnn)
target_include_directories(armnnRefBackendUnitTests PRIVATE ${PROJECT_SOURCE_DIR}/src/armnnUtils)
target_include_directories(armnnRefBackendUnitTests PRIVATE ${PROJECT_SOURCE_DIR}/src/backends)
