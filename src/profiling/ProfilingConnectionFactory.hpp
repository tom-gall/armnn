//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "IProfilingConnectionFactory.hpp"

#include <Runtime.hpp>

#include <memory>

namespace armnn
{

namespace profiling
{

class ProfilingConnectionFactory final : public IProfilingConnectionFactory
{
public:
    ProfilingConnectionFactory()  = default;
    ~ProfilingConnectionFactory() = default;

    IProfilingConnectionPtr GetProfilingConnection(const ExternalProfilingOptions& options) const override;
};

} // namespace profiling

} // namespace armnn
