// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_PROFILE_SESSION_DURATIONS_SERVICE_H_
#define CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_PROFILE_SESSION_DURATIONS_SERVICE_H_

#include "base/scoped_observer.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/driver/sync_service_observer.h"

namespace metrics {

// Tracks the active browsing time that the user spends signed in and/or syncing
// as fraction of their total browsing time.
class DesktopProfileSessionDurationsService
    : public KeyedService,
      public DesktopSessionDurationTracker::Observer,
      public GaiaCookieManagerService::Observer,
      public syncer::SyncServiceObserver {
 public:
  // Callers must ensure that the parameters outlive this object.
  DesktopProfileSessionDurationsService(
      browser_sync::ProfileSyncService* sync_service,
      GaiaCookieManagerService* cookie_manager,
      DesktopSessionDurationTracker* tracker);
  ~DesktopProfileSessionDurationsService() override;

  // DesktopSessionDurationtracker::Observer:
  void OnSessionStarted(base::TimeTicks session_start) override;
  void OnSessionEnded(base::TimeDelta session_length) override;

  // GaiaCookieManagerService::Observer:
  void OnGaiaAccountsInCookieUpdated(
      const std::vector<gaia::ListedAccount>& accounts,
      const std::vector<gaia::ListedAccount>& signed_out_accounts,
      const GoogleServiceAuthError& error) override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  // The state the feature is in. The state starts as UNKNOWN. After it moves
  // out of UNKNOWN, it can alternate between OFF and ON.
  enum class FeatureState { UNKNOWN, OFF, ON };

  // KeyedService:
  void Shutdown() override;

  FeatureState GetSigninStatus(const std::vector<gaia::ListedAccount>& accounts,
                               const GoogleServiceAuthError& error);

  FeatureState GetSyncStatus(const syncer::SyncService* sync);

  void LogSigninDuration(base::TimeDelta session_length);

  void LogSyncDuration(base::TimeDelta session_length);

  ScopedObserver<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observer_;
  ScopedObserver<GaiaCookieManagerService, GaiaCookieManagerService::Observer>
      gaia_cookie_observer_;
  ScopedObserver<DesktopSessionDurationTracker,
                 DesktopSessionDurationTracker::Observer>
      session_duration_observer_;

  // Tracks the elapsed active session time while the browser is open. The timer
  // is absent if there's no active session.
  std::unique_ptr<base::ElapsedTimer> total_session_timer_;

  FeatureState signin_status_ = FeatureState::UNKNOWN;
  // Tracks the elapsed active session time in the current signin status. The
  // timer is absent if there's no active session.
  std::unique_ptr<base::ElapsedTimer> signin_session_timer_;

  FeatureState sync_status_ = FeatureState::UNKNOWN;
  // Tracks the elapsed active session time in the current sync status. The
  // timer is absent if there's no active session.
  std::unique_ptr<base::ElapsedTimer> sync_session_timer_;

  DISALLOW_COPY_AND_ASSIGN(DesktopProfileSessionDurationsService);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_PROFILE_SESSION_DURATIONS_SERVICE_H_
