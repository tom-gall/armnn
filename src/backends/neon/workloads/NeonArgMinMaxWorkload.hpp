//
// Copyright © 2019 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <backendsCommon/Workload.hpp>

#include <arm_compute/core/Error.h>
#include <arm_compute/runtime/NEON/functions/NEArgMinMaxLayer.h>
namespace armnn
{

arm_compute::Status NeonArgMinMaxWorkloadValidate(const TensorInfo& input,
                                                  const TensorInfo& output,
                                                  const ArgMinMaxDescriptor& descriptor);

class NeonArgMinMaxWorkload : public BaseWorkload<ArgMinMaxQueueDescriptor>
{
public:
    NeonArgMinMaxWorkload(const ArgMinMaxQueueDescriptor& descriptor, const WorkloadInfo& info);
    virtual void Execute() const override;

private:
    mutable arm_compute::NEArgMinMaxLayer m_ArgMinMaxLayer;
};

} //namespace armnn
