#pragma once

#include <cstdint>
#include <functional>
#define min // workaround: nimble's min/max macros conflict with libstdc++
#define max
#include <host/ble_gap.h>
#undef max
#undef min
#include "components/ble/BleClient.h"
#include <unordered_map>
#include <string>

namespace Pinetime {

  namespace System {
    class SystemTask;
  }

  namespace Controllers {
    class NotificationManager;

    class AppleMediaServiceClient : public BleClient {
    public:
      explicit AppleMediaServiceClient(Pinetime::System::SystemTask& systemTask,
                                       Pinetime::Controllers::NotificationManager& notificationManager);

      bool OnDiscoveryEvent(uint16_t connectionHandle, const ble_gatt_error* error, const ble_gatt_svc* service);
      int OnCharacteristicsDiscoveryEvent(uint16_t connectionHandle, const ble_gatt_error* error, const ble_gatt_chr* characteristic);
      int OnNewAlertSubcribe(uint16_t connectionHandle, const ble_gatt_error* error, ble_gatt_attr* attribute);
      int OnDescriptorDiscoveryEventCallback(uint16_t connectionHandle,
                                             const ble_gatt_error* error,
                                             uint16_t characteristicValueHandle,
                                             const ble_gatt_dsc* descriptor);
      int OnControlPointWrite(uint16_t connectionHandle, const ble_gatt_error* error, ble_gatt_attr* attribute);
      void OnNotification(ble_gap_event* event);
      void Reset();
      void Discover(uint16_t connectionHandle, std::function<void(uint16_t)> lambda) override;

      void AppleMediaServiceClient::DebugNotification(const char* msg) const;

      std::string getArtist() const{
        return artist;
      };
      std::string getTrack() const {
        return track;
      };
      std::string getAlbum() const {
        return album;
      };
      int getProgress() const {
        return progress;
      };
      int getTrackLength() const {
        return trackLength;
      };
      float getPlaybackSpeed() const {
        return playbackSpeed;
      };
      bool isPlaying() const {
        return isPlaying;
      };

      // 89D3502B-0F36-433A-8EF4-C502AD55F8DC
      static constexpr ble_uuid128_t amsUuid {
        .u {.type = BLE_UUID_TYPE_128},
        .value = {0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5, 0xF4, 0x8E, 0x3A, 0x43, 0x36, 0x0F, 0x2B, 0x50, 0xD3, 0x89}};

    private:
      // 9B3C81D8-57B1-4A8A-B8DF-0E56F7CA51C2
      const ble_uuid128_t remoteCommandChar {
        .u {.type = BLE_UUID_TYPE_128},
        .value = {0xC2, 0x51, 0xCA, 0xF7, 0x56, 0x0E, 0xDF, 0xB8, 0x8A, 0x4A, 0xB1, 0x57, 0xD8, 0x81, 0x3C, 0x9B}};
      // 2F7CABCE-808D-411F-9A0C-BB92BA96C102
      const ble_uuid128_t entityUpdateChar {
        .u {.type = BLE_UUID_TYPE_128},
        .value = {0x02, 0xC1, 0x96, 0xBA, 0x92, 0xBB, 0x0C, 0x9A, 0x1F, 0x41, 0x8D, 0x80, 0xCE, 0xAB, 0x7C, 0x2F}};
      // C6B2F38C-23AB-46D8-A6AB-A3A870BBD5D7
      const ble_uuid128_t entityAttributeChar {
        .u {.type = BLE_UUID_TYPE_128},
        .value = {0xD7, 0xD5, 0xBB, 0x70, 0xA8, 0xA3, 0xAB, 0xA6, 0xD8, 0x46, 0xAB, 0x23, 0x8C, 0xF3, 0xB2, 0xC6}};

      std::string artist;
      std::string track;
      std::string album;
      int progress;
      int trackLength;
      float playbackSpeed;
      bool isPlaying;

      uint16_t amsStartHandle {0};
      uint16_t amsEndHandle {0};
      uint16_t remoteCommandHandle {0};
      uint16_t entityUpdateHandle {0};
      uint16_t entityAttributeHandle {0};
      uint16_t remoteCommandDescriptorHandle {0};
      uint16_t entityUpdateDescriptorHandle {0};
      uint16_t entityAttributeDescriptorHandle {0};

      bool isDiscovered {false};
      bool isRemoteCommandCharacteristicDiscovered {false};
      bool isRemoteCommandDescriptorFound {false};
      bool isEntityUpdateCharacteristicDiscovered {false};
      bool isEntityUpdateDescriptorFound {false};
      bool isEntityAttributeCharacteristicDiscovered {false};
      Pinetime::System::SystemTask& systemTask;
      Pinetime::Controllers::NotificationManager& notificationManager;
      std::function<void(uint16_t)> onServiceDiscovered;
    };
  }
}