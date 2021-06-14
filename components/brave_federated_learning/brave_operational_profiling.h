/* Copyright 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_FEDERATED_LEARNING_BRAVE_OPERATIONAL_PROFILING_H_
#define BRAVE_COMPONENTS_BRAVE_FEDERATED_LEARNING_BRAVE_OPERATIONAL_PROFILING_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace net {
class HttpResponseHeaders;
}

namespace network {

class SharedURLLoaderFactory;
class SimpleURLLoader;

}  // namespace network

namespace brave {

class BraveOperationalProfiling final {
 public:
  BraveOperationalProfiling(
      PrefService* pref_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~BraveOperationalProfiling();
  BraveOperationalProfiling(const BraveOperationalProfiling&) = delete;
  BraveOperationalProfiling& operator=(const BraveOperationalProfiling&) =
      delete;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  void Start();
  void Stop();

 private:
  void OnCollectionSlotStartTimerFired();
  void OnSimulateLocalTrainingStepTimerFired();
  void OnUploadComplete(scoped_refptr<net::HttpResponseHeaders> headers);
  void OnPreferenceChanged(const std::string& key);

  void SendCollectionSlot();

  void SavePrefs();
  void LoadPrefs();
  void InitPrefChangeRegistrar();

  std::string BuildPayload() const;
  int GetCurrentCollectionSlot() const;

  void MaybeResetCollectionId();

  PrefService* local_state_;
  PrefChangeRegistrar local_state_change_registrar_;
  std::unique_ptr<base::RepeatingTimer> collection_slot_periodic_timer_;
  std::unique_ptr<base::RetainingOneShotTimer>
      simulate_local_training_step_timer_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::Time collection_id_expiration_time_;
  int current_collected_slot_ = 0;
  int last_checked_slot_ = 0;
  std::string collection_id_;
};

}  // namespace brave

#endif  // BRAVE_COMPONENTS_BRAVE_FEDERATED_LEARNING_BRAVE_OPERATIONAL_PROFILING_H_
