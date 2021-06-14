/* Copyright 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include <string>
#include <utility>

#include "brave/components/brave_federated_learning/brave_operational_profiling.h"

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "brave/components/brave_federated_learning/brave_operational_profiling_features.h"
#include "brave/components/brave_stats/browser/brave_stats_updater_util.h"
#include "brave/components/p3a/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace brave {

namespace {

static constexpr char federatedLearningUrl[] = "https://fl.brave.com/";

constexpr char kLastCheckedSlotPrefName[] = "brave.federated.last_checked_slot";
constexpr char kCollectionIdPrefName[] = "brave.federated.collection_id";
constexpr char kCollectionIdExpirationPrefName[] =
    "brave.federated.collection_id_expiration";

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation("brave_operational_profiling", R"(
        semantics {
          sender: "Operational Profiling Service"
          description:
            "Report of anonymized usage statistics. For more info see "
            "TODO: https://wikilink_here"
          trigger:
            "Reports are automatically generated on startup and at intervals "
            "while Brave is running."
          data:
            "Anonymized and encrypted usage data."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This service is enabled only when P3A is enabled and the user"
            "has opted-in to ads."
          policy_exception_justification:
            "Not implemented."
        }
    )");
}

}  // anonymous namespace

BraveOperationalProfiling::BraveOperationalProfiling(
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : local_state_(pref_service), url_loader_factory_(url_loader_factory) {}

BraveOperationalProfiling::~BraveOperationalProfiling() {}

void BraveOperationalProfiling::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kLastCheckedSlotPrefName, -1);
  registry->RegisterStringPref(kCollectionIdPrefName, {});
  registry->RegisterTimePref(kCollectionIdExpirationPrefName, base::Time());
}

void BraveOperationalProfiling::Start() {
  DCHECK(!simulate_local_training_step_timer_);
  DCHECK(!collection_slot_periodic_timer_);

  LoadPrefs();
  InitPrefChangeRegistrar();
  MaybeResetCollectionId();

  simulate_local_training_step_timer_ =
      std::make_unique<base::RetainingOneShotTimer>();
  simulate_local_training_step_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(
          operational_profiling::features::
              GetSimulateLocalTrainingStepDurationValue() *
          60),
      this, &BraveOperationalProfiling::OnSimulateLocalTrainingStepTimerFired);

  collection_slot_periodic_timer_ = std::make_unique<base::RepeatingTimer>();
  collection_slot_periodic_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(
          operational_profiling::features::GetCollectionSlotSizeValue() * 60 /
          2),
      this, &BraveOperationalProfiling::OnCollectionSlotStartTimerFired);
}

void BraveOperationalProfiling::Stop() {
  simulate_local_training_step_timer_.reset();
  collection_slot_periodic_timer_.reset();
}

void BraveOperationalProfiling::InitPrefChangeRegistrar() {
  local_state_change_registrar_.Init(local_state_);
  local_state_change_registrar_.Add(
      brave::kP3AEnabled,
      base::BindRepeating(&BraveOperationalProfiling::OnPreferenceChanged,
                          base::Unretained(this)));
}

void BraveOperationalProfiling::LoadPrefs() {
  last_checked_slot_ = local_state_->GetInteger(kLastCheckedSlotPrefName);
  collection_id_ = local_state_->GetString(kCollectionIdPrefName);
  collection_id_expiration_time_ =
      local_state_->GetTime(kCollectionIdExpirationPrefName);
}

void BraveOperationalProfiling::SavePrefs() {
  local_state_->SetInteger(kLastCheckedSlotPrefName, last_checked_slot_);
  local_state_->SetString(kCollectionIdPrefName, collection_id_);
  local_state_->SetTime(kCollectionIdExpirationPrefName,
                        collection_id_expiration_time_);
}

void BraveOperationalProfiling::OnPreferenceChanged(const std::string& key) {
  bool p3a_enabled = local_state_->GetBoolean(brave::kP3AEnabled);
  bool is_operational_profiling_enabled =
      operational_profiling::features::IsOperationalProfilingEnabled();
  if (!p3a_enabled || !is_operational_profiling_enabled) {
    Stop();
  }
}

void BraveOperationalProfiling::OnCollectionSlotStartTimerFired() {
  simulate_local_training_step_timer_->Reset();
}

void BraveOperationalProfiling::OnSimulateLocalTrainingStepTimerFired() {
  SendCollectionSlot();
}

void BraveOperationalProfiling::SendCollectionSlot() {
  current_collected_slot_ = GetCurrentCollectionSlot();
  if (current_collected_slot_ == last_checked_slot_) {
    return;
  }

  MaybeResetCollectionId();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(federatedLearningUrl);
  resource_request->headers.SetHeader("X-Brave-FL-Operational-Profile", "?1");

  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), GetNetworkTrafficAnnotationTag());
  url_loader_->AttachStringForUpload(BuildPayload(), "application/json");

  url_loader_->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(&BraveOperationalProfiling::OnUploadComplete,
                     base::Unretained(this)));
}

void BraveOperationalProfiling::OnUploadComplete(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  int response_code = -1;
  if (headers)
    response_code = headers->response_code();
  if (response_code == 200) {
    last_checked_slot_ = current_collected_slot_;
    SavePrefs();
  }
}

std::string BraveOperationalProfiling::BuildPayload() const {
  base::Value root(base::Value::Type::DICTIONARY);

  root.SetKey("collection_id", base::Value(collection_id_));
  root.SetKey("platform", base::Value(brave_stats::GetPlatformIdentifier()));
  root.SetKey("collection_slot", base::Value(current_collected_slot_));

  std::string result;
  base::JSONWriter::Write(root, &result);

  return result;
}

int BraveOperationalProfiling::GetCurrentCollectionSlot() const {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);

  return ((now.day_of_month - 1) * 24 * 60 + now.hour * 60 + now.minute) /
         operational_profiling::features::GetCollectionSlotSizeValue();
}

void BraveOperationalProfiling::MaybeResetCollectionId() {
  const base::Time now = base::Time::Now();
  if (collection_id_.empty() || (!collection_id_expiration_time_.is_null() &&
                                 now > collection_id_expiration_time_)) {
    collection_id_ =
        base::ToUpperASCII(base::UnguessableToken::Create().ToString());
    collection_id_expiration_time_ =
        now + base::TimeDelta::FromSeconds(
                  operational_profiling::features::GetCollectionIdLifetime() *
                  24 * 60 * 60);
    SavePrefs();
  }
}

}  // namespace brave
