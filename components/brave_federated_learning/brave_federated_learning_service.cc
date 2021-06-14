/* Copyright 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_federated_learning/brave_federated_learning_service.h"

#include "bat/ads/pref_names.h"
#include "brave/components/brave_federated_learning/brave_operational_profiling.h"
#include "brave/components/brave_federated_learning/brave_operational_profiling_features.h"
#include "brave/components/p3a/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace brave {

BraveFederatedLearningService::BraveFederatedLearningService(
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : local_state_(pref_service), url_loader_factory_(url_loader_factory) {}

BraveFederatedLearningService::~BraveFederatedLearningService() {}

void BraveFederatedLearningService::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  BraveOperationalProfiling::RegisterLocalStatePrefs(registry);
}

void BraveFederatedLearningService::Start() {
  if (isAdsEnabled() && isP3AEnabled() && isOperationalProfilingEnabled()) {
    operational_profiling_.reset(
        new BraveOperationalProfiling(local_state_, url_loader_factory_));
    operational_profiling_->Start();
  }
}

bool BraveFederatedLearningService::isOperationalProfilingEnabled() {
  return operational_profiling::features::IsOperationalProfilingEnabled();
}

bool BraveFederatedLearningService::isAdsEnabled() {
  // TODO(lminto): is pegging to ads necessary?
  return true;
}

bool BraveFederatedLearningService::isP3AEnabled() {
  return local_state_->GetBoolean(brave::kP3AEnabled);
}

}  // namespace brave
