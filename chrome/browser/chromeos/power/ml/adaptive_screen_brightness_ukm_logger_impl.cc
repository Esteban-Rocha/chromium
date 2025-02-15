// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/adaptive_screen_brightness_ukm_logger_impl.h"

#include <cmath>

#include "base/logging.h"
#include "chrome/browser/chromeos/power/ml/screen_brightness_event.pb.h"
#include "chrome/browser/chromeos/power/ml/user_activity_ukm_logger_impl.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace chromeos {
namespace power {
namespace ml {

namespace {

constexpr int kSecondsPerHour = 3600;

constexpr UserActivityUkmLoggerImpl::Bucket kBatteryPercentBuckets[] = {
    {100, 5}};

constexpr UserActivityUkmLoggerImpl::Bucket kUserInputEventBuckets[] = {
    {100, 1},
    {1000, 100},
    {10000, 1000}};

constexpr UserActivityUkmLoggerImpl::Bucket kRecentTimeActiveBuckets[] =
    {{60, 1}, {600, 60}, {1200, 300}, {3600, 600}, {18000, 1800}};

}  // namespace

AdaptiveScreenBrightnessUkmLoggerImpl::
    ~AdaptiveScreenBrightnessUkmLoggerImpl() = default;

void AdaptiveScreenBrightnessUkmLoggerImpl::LogActivity(
    const ScreenBrightnessEvent& screen_brightness_event,
    ukm::SourceId tab_id,
    bool has_form_entry) {
  const ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::builders::ScreenBrightness ukm_screen_brightness(source_id);
  ukm_screen_brightness.SetSequenceId(next_sequence_id_++);

  const ScreenBrightnessEvent_Features features =
      screen_brightness_event.features();
  const ScreenBrightnessEvent_Features_ActivityData activity_data =
      features.activity_data();

  if (activity_data.has_time_of_day_sec()) {
    ukm_screen_brightness.SetHourOfDay(
        std::floor(activity_data.time_of_day_sec() / kSecondsPerHour));
  }

  if (activity_data.has_day_of_week()) {
    ukm_screen_brightness.SetDayOfWeek(activity_data.day_of_week());
  }

  if (activity_data.has_num_recent_mouse_events()) {
    ukm_screen_brightness.SetNumRecentMouseEvents(
        UserActivityUkmLoggerImpl::Bucketize(
            activity_data.num_recent_mouse_events(), kUserInputEventBuckets,
            arraysize(kUserInputEventBuckets)));
  }

  if (activity_data.has_num_recent_key_events()) {
    ukm_screen_brightness.SetNumRecentKeyEvents(
        UserActivityUkmLoggerImpl::Bucketize(
            activity_data.num_recent_key_events(), kUserInputEventBuckets,
            arraysize(kUserInputEventBuckets)));
  }

  if (activity_data.has_num_recent_stylus_events()) {
    ukm_screen_brightness.SetNumRecentStylusEvents(
        UserActivityUkmLoggerImpl::Bucketize(
            activity_data.num_recent_stylus_events(), kUserInputEventBuckets,
            arraysize(kUserInputEventBuckets)));
  }

  if (activity_data.has_num_recent_touch_events()) {
    ukm_screen_brightness.SetNumRecentTouchEvents(
        UserActivityUkmLoggerImpl::Bucketize(
            activity_data.num_recent_touch_events(), kUserInputEventBuckets,
            arraysize(kUserInputEventBuckets)));
  }

  if (activity_data.has_last_activity_time_sec()) {
    ukm_screen_brightness.SetLastActivityTimeSec(
        activity_data.last_activity_time_sec());
  }

  if (activity_data.has_recent_time_active_sec()) {
    ukm_screen_brightness.SetRecentTimeActiveSec(
        UserActivityUkmLoggerImpl::Bucketize(
            activity_data.recent_time_active_sec(), kRecentTimeActiveBuckets,
            arraysize(kRecentTimeActiveBuckets)));
  }

  if (activity_data.has_is_video_playing()) {
    ukm_screen_brightness.SetIsVideoPlaying(activity_data.is_video_playing());
  }

  const ScreenBrightnessEvent_Features_EnvData env_data = features.env_data();

  if (env_data.has_on_battery()) {
    ukm_screen_brightness.SetOnBattery(env_data.on_battery());
  }

  if (env_data.has_battery_percent()) {
    ukm_screen_brightness.SetBatteryPercent(
        UserActivityUkmLoggerImpl::Bucketize(
            std::floor(env_data.battery_percent()), kBatteryPercentBuckets,
            arraysize(kBatteryPercentBuckets)));
  }

  if (env_data.has_device_mode()) {
    ukm_screen_brightness.SetDeviceMode(env_data.device_mode());
  }

  if (env_data.has_night_light_temperature_percent()) {
    ukm_screen_brightness.SetNightLightTemperaturePercent(
        env_data.night_light_temperature_percent());
  }

  const ScreenBrightnessEvent_Features_AccessibilityData accessibility_data =
      features.accessibility_data();

  if (accessibility_data.has_is_magnifier_enabled()) {
    ukm_screen_brightness.SetIsMagnifierEnabled(
        accessibility_data.is_magnifier_enabled());
  }

  if (accessibility_data.has_is_high_contrast_enabled()) {
    ukm_screen_brightness.SetIsHighContrastEnabled(
        accessibility_data.is_high_contrast_enabled());
  }

  if (accessibility_data.has_is_large_cursor_enabled()) {
    ukm_screen_brightness.SetIsLargeCursorEnabled(
        accessibility_data.is_large_cursor_enabled());
  }

  if (accessibility_data.has_is_virtual_keyboard_enabled()) {
    ukm_screen_brightness.SetIsVirtualKeyboardEnabled(
        accessibility_data.is_virtual_keyboard_enabled());
  }

  if (accessibility_data.has_is_spoken_feedback_enabled()) {
    ukm_screen_brightness.SetIsSpokenFeedbackEnabled(
        accessibility_data.is_spoken_feedback_enabled());
  }

  if (accessibility_data.has_is_select_to_speak_enabled()) {
    ukm_screen_brightness.SetIsSelectToSpeakEnabled(
        accessibility_data.is_select_to_speak_enabled());
  }

  if (accessibility_data.has_is_mono_audio_enabled()) {
    ukm_screen_brightness.SetIsMonoAudioEnabled(
        accessibility_data.is_mono_audio_enabled());
  }

  if (accessibility_data.has_is_caret_highlight_enabled()) {
    ukm_screen_brightness.SetIsCaretHighlightEnabled(
        accessibility_data.is_caret_highlight_enabled());
  }

  if (accessibility_data.has_is_cursor_highlight_enabled()) {
    ukm_screen_brightness.SetIsCursorHighlightEnabled(
        accessibility_data.is_cursor_highlight_enabled());
  }

  if (accessibility_data.has_is_focus_highlight_enabled()) {
    ukm_screen_brightness.SetIsFocusHighlightEnabled(
        accessibility_data.is_focus_highlight_enabled());
  }

  if (accessibility_data.has_is_braille_display_connected()) {
    ukm_screen_brightness.SetIsBrailleDisplayConnected(
        accessibility_data.is_braille_display_connected());
  }

  if (accessibility_data.has_is_autoclick_enabled()) {
    ukm_screen_brightness.SetIsAutoclickEnabled(
        accessibility_data.is_autoclick_enabled());
  }

  if (accessibility_data.has_is_switch_access_enabled()) {
    ukm_screen_brightness.SetIsSwitchAccessEnabled(
        accessibility_data.is_switch_access_enabled());
  }

  const ScreenBrightnessEvent_Event event = screen_brightness_event.event();

  DCHECK(event.brightness());
  ukm_screen_brightness.SetBrightness(event.brightness());

  if (event.has_reason()) {
    ukm_screen_brightness.SetReason(event.reason());
  }

  ukm::UkmRecorder* const ukm_recorder = ukm::UkmRecorder::Get();
  ukm_screen_brightness.Record(ukm_recorder);

  if (tab_id == ukm::kInvalidSourceId)
    return;

  ukm::builders::UserActivityId user_activity_id(tab_id);
  user_activity_id.SetActivityId(source_id).SetHasFormEntry(has_form_entry);
  user_activity_id.Record(ukm_recorder);
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
