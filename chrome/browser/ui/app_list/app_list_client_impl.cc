// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_client_impl.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "ash/public/cpp/menu_utils.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/app_sync_ui_state_watcher.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/search_controller_factory.h"
#include "chrome/browser/ui/app_list/search/search_resource_manager.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/models/menu_model.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

AppListClientImpl::AppListClientImpl()
    : template_url_service_observer_(this),
      binding_(this),
      weak_ptr_factory_(this) {
  // Bind this to the AppListController in Ash.
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &app_list_controller_);
  ash::mojom::AppListClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  app_list_controller_->SetClient(std::move(client));

  AppListServiceImpl::GetInstance()->SetAppListControllerAndClient(
      app_list_controller_.get(), this);
}

AppListClientImpl::~AppListClientImpl() = default;

void AppListClientImpl::StartSearch(const base::string16& raw_query) {
  if (search_controller_) {
    search_controller_->Start(raw_query);
    controller_delegate_->OnSearchStarted();
  }
}

void AppListClientImpl::OpenSearchResult(const std::string& result_id,
                                         int event_flags) {
  if (!model_updater_)
    return;
  ChromeSearchResult* result = model_updater_->FindSearchResult(result_id);
  if (result)
    search_controller_->OpenResult(result, event_flags);
}

void AppListClientImpl::InvokeSearchResultAction(const std::string& result_id,
                                                 int action_index,
                                                 int event_flags) {
  if (!model_updater_)
    return;
  ChromeSearchResult* result = model_updater_->FindSearchResult(result_id);
  if (result)
    search_controller_->InvokeResultAction(result, action_index, event_flags);
}

void AppListClientImpl::ViewClosing() {
  controller_delegate_->SetAppListDisplayId(display::kInvalidDisplayId);
}

void AppListClientImpl::ViewShown(int64_t display_id) {
  if (model_updater_) {
    base::RecordAction(base::UserMetricsAction("Launcher_Show"));
    base::UmaHistogramSparse("Apps.AppListBadgedAppsCount",
                             model_updater_->BadgedItemCount());
  }
  controller_delegate_->SetAppListDisplayId(display_id);
}

void AppListClientImpl::ActivateItem(const std::string& id, int event_flags) {
  if (!model_updater_)
    return;
  model_updater_->ActivateChromeItem(id, event_flags);
}

void AppListClientImpl::GetContextMenuModel(
    const std::string& id,
    GetContextMenuModelCallback callback) {
  if (!model_updater_)
    return;
  ui::MenuModel* menu = model_updater_->GetContextMenuModel(id);
  std::move(callback).Run(ash::menu_utils::GetMojoMenuItemsFromModel(menu));
}

void AppListClientImpl::ContextMenuItemSelected(const std::string& id,
                                                int command_id,
                                                int event_flags) {
  if (!model_updater_)
    return;
  model_updater_->ContextMenuItemSelected(id, command_id, event_flags);
}

void AppListClientImpl::OnAppListTargetVisibilityChanged(bool visible) {
  AppListServiceImpl::GetInstance()->set_app_list_target_visible(visible);
}

void AppListClientImpl::OnAppListVisibilityChanged(bool visible) {
  AppListServiceImpl::GetInstance()->set_app_list_visible(visible);
}

void AppListClientImpl::StartVoiceInteractionSession() {
  auto* service =
      arc::ArcVoiceInteractionFrameworkService::GetForBrowserContext(
          ChromeLauncherController::instance()->profile());
  if (service)
    service->StartSessionFromUserInteraction(gfx::Rect());
}

void AppListClientImpl::ToggleVoiceInteractionSession() {
  auto* service =
      arc::ArcVoiceInteractionFrameworkService::GetForBrowserContext(
          ChromeLauncherController::instance()->profile());
  if (service)
    service->ToggleSessionFromUserInteraction();
}

void AppListClientImpl::OnFolderCreated(
    ash::mojom::AppListItemMetadataPtr item) {
  if (!model_updater_)
    return;
  DCHECK(item->is_folder);
  model_updater_->OnFolderCreated(std::move(item));
}

void AppListClientImpl::OnFolderDeleted(
    ash::mojom::AppListItemMetadataPtr item) {
  if (!model_updater_)
    return;
  DCHECK(item->is_folder);
  model_updater_->OnFolderDeleted(std::move(item));
}

void AppListClientImpl::OnItemUpdated(ash::mojom::AppListItemMetadataPtr item) {
  if (!model_updater_)
    return;
  model_updater_->OnItemUpdated(std::move(item));
}

void AppListClientImpl::UpdateProfile() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  app_list::AppListSyncableService* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  DCHECK(syncable_service);
  SetProfile(profile);
}

void AppListClientImpl::SetProfile(Profile* new_profile) {
  if (profile_ == new_profile)
    return;

  if (profile_) {
    DCHECK(model_updater_);
    model_updater_->SetActive(false);

    search_resource_manager_.reset();
    search_controller_.reset();
    app_sync_ui_state_watcher_.reset();
    model_updater_ = nullptr;
  }

  template_url_service_observer_.RemoveAll();

  profile_ = new_profile;
  if (!profile_)
    return;

  // If we are in guest mode, the new profile should be an incognito profile.
  // Otherwise, this may later hit a check (same condition as this one) in
  // Browser::Browser when opening links in a browser window (see
  // http://crbug.com/460437).
  DCHECK(!profile_->IsGuestSession() || profile_->IsOffTheRecord())
      << "Guest mode must use incognito profile";

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  template_url_service_observer_.Add(template_url_service);

  app_list::AppListSyncableService* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
  model_updater_ = syncable_service->GetModelUpdater();
  model_updater_->SetActive(true);

  app_sync_ui_state_watcher_ =
      std::make_unique<AppSyncUIStateWatcher>(profile_, model_updater_);

  SetUpSearchUI();
  OnTemplateURLServiceChanged();

  // Clear search query.
  model_updater_->UpdateSearchBox(base::string16(),
                                  false /* initiated_by_user */);
}

void AppListClientImpl::SetUpSearchUI() {
  search_resource_manager_.reset(
      new app_list::SearchResourceManager(profile_, model_updater_));

  search_controller_ = app_list::CreateSearchController(
      profile_, model_updater_, controller_delegate_);
}

void AppListClientImpl::OnTemplateURLServiceChanged() {
  DCHECK(model_updater_);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  const bool is_google =
      default_provider &&
      default_provider->GetEngineType(
          template_url_service->search_terms_data()) == SEARCH_ENGINE_GOOGLE;

  model_updater_->SetSearchEngineIsGoogle(is_google);
}

void AppListClientImpl::FlushMojoForTesting() {
  app_list_controller_.FlushForTesting();
  binding_.FlushForTesting();
}
