// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_host_mojo.h"

#include <string>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screens/gaia_view.h"
#include "chrome/browser/chromeos/login/ui/gaia_dialog_delegate.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/chromeos/login/ui/login_display_mojo.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/user_names.h"

namespace chromeos {

LoginDisplayHostMojo::LoginDisplayHostMojo() : weak_factory_(this) {
  // Preload the WebUI for post-login screens.
  InitWidgetAndView();
}

LoginDisplayHostMojo::~LoginDisplayHostMojo() {
  LoginScreenClient::Get()->SetDelegate(nullptr);
  if (dialog_)
    dialog_->Close();
}

void LoginDisplayHostMojo::OnDialogDestroyed(const GaiaDialogDelegate* dialog) {
  if (dialog == dialog_) {
    dialog_ = nullptr;
    wizard_controller_.reset();
  }
}

void LoginDisplayHostMojo::SetUsers(const user_manager::UserList& users) {
  users_ = users;
}

LoginDisplay* LoginDisplayHostMojo::CreateLoginDisplay(
    LoginDisplay::Delegate* delegate) {
  return new LoginDisplayMojo(delegate, this);
}

gfx::NativeWindow LoginDisplayHostMojo::GetNativeWindow() const {
  NOTIMPLEMENTED();
  return nullptr;
}

OobeUI* LoginDisplayHostMojo::GetOobeUI() const {
  if (!dialog_)
    return nullptr;
  return dialog_->GetOobeUI();
}

WebUILoginView* LoginDisplayHostMojo::GetWebUILoginView() const {
  NOTREACHED();
  return nullptr;
}

void LoginDisplayHostMojo::OnFinalize() {
  if (dialog_)
    dialog_->Close();

  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void LoginDisplayHostMojo::SetStatusAreaVisible(bool visible) {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::StartWizard(OobeScreen first_screen) {
  DCHECK(GetOobeUI());

  // Dtor of the old WizardController should be called before ctor of the
  // new one to ensure only one |ExistingUserController| instance at a time.
  wizard_controller_.reset();
  wizard_controller_.reset(new WizardController(this, GetOobeUI()));
  wizard_controller_->Init(first_screen);

  // Post login screens should not be closable by escape key.
  dialog_->Show(false /*closable_by_esc*/);
}

WizardController* LoginDisplayHostMojo::GetWizardController() {
  NOTIMPLEMENTED();
  return nullptr;
}

void LoginDisplayHostMojo::OnStartUserAdding() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::CancelUserAdding() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::OnStartSignInScreen(
    const LoginScreenContext& context) {
  // This function may be called early in startup flow, before LoginScreenClient
  // has been initialized. Wait until LoginScreenClient is initialized as it is
  // a common dependency.
  if (!LoginScreenClient::HasInstance()) {
    // TODO(jdufault): Add a timeout here / make sure we do not post infinitely.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&LoginDisplayHostMojo::OnStartSignInScreen,
                                  weak_factory_.GetWeakPtr(), context));
    return;
  }

  // There can only be one |ExistingUserController| instance at a time.
  existing_user_controller_.reset();
  existing_user_controller_ = std::make_unique<ExistingUserController>(this);

  // We need auth attempt results to notify views-based lock screen.
  existing_user_controller_->set_login_status_consumer(this);

  // Load the UI.
  existing_user_controller_->Init(user_manager::UserManager::Get()->GetUsers());
}

void LoginDisplayHostMojo::OnPreferencesChanged() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::OnStartAppLaunch() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::OnStartArcKiosk() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::OnBrowserCreated() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::StartVoiceInteractionOobe() {
  NOTIMPLEMENTED();
}

bool LoginDisplayHostMojo::IsVoiceInteractionOobe() {
  NOTIMPLEMENTED();
  return false;
}

void LoginDisplayHostMojo::UpdateGaiaDialogVisibility(bool visible) {
  DCHECK(dialog_);
  if (visible) {
    dialog_->Show(true /*closable_by_esc*/);
    return;
  }

  if (users_.empty() && GetOobeUI()) {
    // Dialog could not be closed if there's no user in the login screen,
    // refreshing the dialog instead.
    GetOobeUI()->GetGaiaScreenView()->ShowGaiaAsync();
    return;
  }

  dialog_->Hide();
}

void LoginDisplayHostMojo::UpdateGaiaDialogSize(int width, int height) {
  if (dialog_)
    dialog_->SetSize(width, height);
}

const user_manager::UserList LoginDisplayHostMojo::GetUsers() {
  return users_;
}

void LoginDisplayHostMojo::HandleAuthenticateUser(
    const AccountId& account_id,
    const std::string& hashed_password,
    const password_manager::SyncPasswordData& sync_password_data,
    bool authenticated_by_pin,
    AuthenticateUserCallback callback) {
  DCHECK(!authenticated_by_pin);
  DCHECK_EQ(account_id.GetUserEmail(),
            gaia::SanitizeEmail(account_id.GetUserEmail()));

  on_authenticated_ = std::move(callback);

  UserContext user_context(account_id);
  user_context.SetKey(Key(chromeos::Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                          std::string(), hashed_password));
  user_context.SetSyncPasswordData(sync_password_data);
  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY)
    user_context.SetUserType(user_manager::USER_TYPE_ACTIVE_DIRECTORY);

  existing_user_controller_->Login(user_context, chromeos::SigninSpecifics());
}

void LoginDisplayHostMojo::HandleAttemptUnlock(const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::HandleHardlockPod(const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::HandleRecordClickOnLockIcon(
    const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::HandleOnFocusPod(const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::HandleOnNoPodFocused() {
  NOTIMPLEMENTED();
}

bool LoginDisplayHostMojo::HandleFocusLockScreenApps(bool reverse) {
  NOTIMPLEMENTED();
  return false;
}

void LoginDisplayHostMojo::HandleLoginAsGuest() {
  existing_user_controller_->Login(UserContext(user_manager::USER_TYPE_GUEST,
                                               user_manager::GuestAccountId()),
                                   chromeos::SigninSpecifics());
}

void LoginDisplayHostMojo::HandleLaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT, account_id);
  context.SetPublicSessionLocale(locale);
  context.SetPublicSessionInputMethod(input_method);
  existing_user_controller_->Login(context, chromeos::SigninSpecifics());
}

void LoginDisplayHostMojo::OnAuthFailure(const AuthFailure& error) {
  if (on_authenticated_)
    std::move(on_authenticated_).Run(false);
}

void LoginDisplayHostMojo::OnAuthSuccess(const UserContext& user_context) {
  if (on_authenticated_)
    std::move(on_authenticated_).Run(true);
}

void LoginDisplayHostMojo::InitWidgetAndView() {
  if (dialog_)
    return;

  dialog_ = new GaiaDialogDelegate(weak_factory_.GetWeakPtr());
  dialog_->Init();
}

}  // namespace chromeos
