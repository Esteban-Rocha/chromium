// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_CLIENT_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "ash/public/interfaces/app_list.mojom.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace app_list {
class SearchController;
class SearchResourceManager;
}  // namespace app_list

class AppListControllerDelegate;
class AppListModelUpdater;
class AppSyncUIStateWatcher;
class Profile;

class AppListClientImpl : public ash::mojom::AppListClient,
                          public TemplateURLServiceObserver {
 public:
  AppListClientImpl();
  ~AppListClientImpl() override;

  // ash::mojom::AppListClient:
  void StartSearch(const base::string16& raw_query) override;
  void OpenSearchResult(const std::string& result_id, int event_flags) override;
  void InvokeSearchResultAction(const std::string& result_id,
                                int action_index,
                                int event_flags) override;
  void ViewClosing() override;
  void ViewShown(int64_t display_id) override;
  void ActivateItem(const std::string& id, int event_flags) override;
  void GetContextMenuModel(const std::string& id,
                           GetContextMenuModelCallback callback) override;
  void ContextMenuItemSelected(const std::string& id,
                               int command_id,
                               int event_flags) override;

  void OnAppListTargetVisibilityChanged(bool visible) override;
  void OnAppListVisibilityChanged(bool visible) override;
  void StartVoiceInteractionSession() override;
  void ToggleVoiceInteractionSession() override;

  void OnFolderCreated(ash::mojom::AppListItemMetadataPtr item) override;
  void OnFolderDeleted(ash::mojom::AppListItemMetadataPtr item) override;
  void OnItemUpdated(ash::mojom::AppListItemMetadataPtr item) override;

  // Associates this client with the current active user, called when this
  // client is accessed.
  void UpdateProfile();

  void set_controller_delegate(AppListControllerDelegate* controller_delegate) {
    controller_delegate_ = controller_delegate;
  }

  // Flushes all pending mojo call to Ash for testing.
  void FlushMojoForTesting();

 private:
  // Overridden from TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

  // Configures the AppList for the given |profile|.
  void SetProfile(Profile* profile);

  // Updates the speech webview and start page for the current |profile_|.
  void SetUpSearchUI();

  // Unowned pointer to the controller delegate.
  AppListControllerDelegate* controller_delegate_ = nullptr;
  // Unowned pointer to the associated profile. May change if SetProfile is
  // called.
  Profile* profile_ = nullptr;
  // Unowned pointer to the model updater owned by AppListSyncableService.
  // Will change if |profile_| changes.
  AppListModelUpdater* model_updater_ = nullptr;

  std::unique_ptr<app_list::SearchResourceManager> search_resource_manager_;
  std::unique_ptr<app_list::SearchController> search_controller_;
  std::unique_ptr<AppSyncUIStateWatcher> app_sync_ui_state_watcher_;

  ScopedObserver<TemplateURLService, AppListClientImpl>
      template_url_service_observer_;

  mojo::Binding<ash::mojom::AppListClient> binding_;
  ash::mojom::AppListControllerPtr app_list_controller_;

  base::WeakPtrFactory<AppListClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListClientImpl);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_CLIENT_IMPL_H_
