/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/state_pool/StatePool.h>
#include <igl/DepthStencilState.h>

namespace igl {
class IDevice;
} // namespace igl

namespace iglu::state_pool {

class DepthStencilStatePool
  : public LRUStatePool<igl::DepthStencilStateDesc, igl::IDepthStencilState> {
 public:
 private:
  std::shared_ptr<igl::IDepthStencilState> createStateObject(igl::IDevice& dev,
                                                             const igl::DepthStencilStateDesc& desc,
                                                             igl::Result* outResult) override;
};

} // namespace iglu::state_pool
