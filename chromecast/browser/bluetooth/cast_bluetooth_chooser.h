// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_BLUETOOTH_CAST_BLUETOOTH_CHOOSER_H_
#define CHROMECAST_BROWSER_BLUETOOTH_CAST_BLUETOOTH_CHOOSER_H_

#include <string>
#include <unordered_set>

#include "chromecast/browser/bluetooth/public/interfaces/web_bluetooth.mojom.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace chromecast {

// This class requests access from a remote BluetoothDeviceAccessProvider
// implemented by the Activity's host. The host will update this client with
// whitelisted devices via GrantAccess(). Meanwhile, the WebBluetooth stack will
// add devices that match the application's filter via AddOrUpdateDevice(). The
// first device which matches both will be selected.
class CastBluetoothChooser : public content::BluetoothChooser,
                             public mojom::BluetoothDeviceAccessProviderClient {
 public:
  // |event_handler| is called when an approved device is discovered, or when
  // the |provider| destroys the connection to this client. |this| may destroy
  // |provider| immediately after requesting access.
  CastBluetoothChooser(content::BluetoothChooser::EventHandler event_handler,
                       mojom::BluetoothDeviceAccessProviderPtr provider);
  ~CastBluetoothChooser() override;

 private:
  // mojom::BluetoothDeviceAccessProviderClient implementation:
  void GrantAccess(const std::string& address) override;
  void GrantAccessToAllDevices() override;

  // content::BluetoothChooser implementation:
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const base::string16& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override;

  // Runs the event_handler and resets the client binding. After this is called,
  // this class should not be used.
  void RunEventHandlerAndResetBinding(content::BluetoothChooser::Event event,
                                      std::string address);

  // Called when the remote connection held by |binding_| is torn down.
  void OnClientConnectionError();

  content::BluetoothChooser::EventHandler event_handler_;
  mojo::Binding<mojom::BluetoothDeviceAccessProviderClient> binding_;
  std::unordered_set<std::string> available_devices_;
  std::unordered_set<std::string> approved_devices_;
  bool all_devices_approved_ = false;

  DISALLOW_COPY_AND_ASSIGN(CastBluetoothChooser);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_BLUETOOTH_CAST_BLUETOOTH_CHOOSER_H_