// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_CHARACTERISTIC_IMPL_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_CHARACTERISTIC_IMPL_H_

#include <atomic>
#include <map>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"

namespace chromecast {
namespace bluetooth {

class GattClientManagerImpl;
class RemoteDevice;

// A proxy for a remote characteristic on a RemoteDevice. Unless otherwise
// specified, all callbacks are run on the caller's thread.
class RemoteCharacteristicImpl : public RemoteCharacteristic {
 public:
  // RemoteCharacteristic impl:
  std::vector<scoped_refptr<RemoteDescriptor>> GetDescriptors() override;
  scoped_refptr<RemoteDescriptor> GetDescriptorByUuid(
      const bluetooth_v2_shlib::Uuid& uuid) override;
  void SetRegisterNotification(bool enable, StatusCallback cb) override;
  void SetNotification(bool enable, StatusCallback cb) override;
  void ReadAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                ReadCallback callback) override;
  void Read(ReadCallback callback) override;
  void WriteAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                 bluetooth_v2_shlib::Gatt::WriteType write_type,
                 const std::vector<uint8_t>& value,
                 StatusCallback callback) override;
  void Write(bluetooth_v2_shlib::Gatt::WriteType write_type,
             const std::vector<uint8_t>& value,
             StatusCallback callback) override;
  bool NotificationEnabled() override;
  const bluetooth_v2_shlib::Gatt::Characteristic& characteristic()
      const override;
  const bluetooth_v2_shlib::Uuid& uuid() const override;
  uint16_t handle() const override;
  bluetooth_v2_shlib::Gatt::Permissions permissions() const override;
  bluetooth_v2_shlib::Gatt::Properties properties() const override;

 private:
  friend class GattClientManagerImpl;
  friend class RemoteDeviceImpl;
  friend class RemoteServiceImpl;

  RemoteCharacteristicImpl(
      RemoteDevice* device,
      base::WeakPtr<GattClientManagerImpl> gatt_client_manager,
      const bluetooth_v2_shlib::Gatt::Characteristic* characteristic,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~RemoteCharacteristicImpl() override;

  std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteDescriptor>>
  CreateDescriptorMap();

  void OnConnectChanged(bool connected);
  void OnReadComplete(bool status, const std::vector<uint8_t>& value);
  void OnWriteComplete(bool status);

  // Weak reference to avoid refcount loop.
  RemoteDevice* const device_;
  const base::WeakPtr<GattClientManagerImpl> gatt_client_manager_;
  const bluetooth_v2_shlib::Gatt::Characteristic* const characteristic_;

  // All bluetooth_v2_shlib calls are run on this task_runner. All members must
  // be accessed on this task_runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Work around http://crbug/831878. This allows notifications on
  // characteristics which do not have a CCCD.
  const std::unique_ptr<bluetooth_v2_shlib::Gatt::Descriptor> fake_cccd_;

  const std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteDescriptor>>
      uuid_to_descriptor_;

  ReadCallback read_callback_;
  StatusCallback write_callback_;

  std::atomic<bool> notification_enabled_{false};

  bool pending_read_ = false;
  bool pending_write_ = false;

  DISALLOW_COPY_AND_ASSIGN(RemoteCharacteristicImpl);
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_CHARACTERISTIC_IMPL_H_
