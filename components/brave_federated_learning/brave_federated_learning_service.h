/* Copyright 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_FEDERATED_LEARNING_BRAVE_FEDERATED_LEARNING_SERVICE_H_
#define BRAVE_COMPONENTS_BRAVE_FEDERATED_LEARNING_BRAVE_FEDERATED_LEARNING_SERVICE_H_

#include <memory>

#include "base/memory/ref_counted.h"

class PrefRegistrySimple;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace brave {

class BraveOperationalProfiling;

class BraveFederatedLearningService final {
 public:
  explicit BraveFederatedLearningService(
      PrefService* pref_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~BraveFederatedLearningService();
  BraveFederatedLearningService(const BraveFederatedLearningService&) = delete;
  BraveFederatedLearningService& operator=(
      const BraveFederatedLearningService&) = delete;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  void Start();

 private:
  bool isP3AEnabled();
  bool isAdsEnabled();
  bool isOperationalProfilingEnabled();

  PrefService* local_state_;
  std::unique_ptr<BraveOperationalProfiling> operational_profiling_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace brave

#endif  // BRAVE_COMPONENTS_BRAVE_FEDERATED_LEARNING_BRAVE_FEDERATED_LEARNING_SERVICE_H_
