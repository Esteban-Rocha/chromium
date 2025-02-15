// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

gfx::ImageSkiaRep ScaleImageSkiaRep(const gfx::ImageSkiaRep& rep,
                                    int target_width_dp,
                                    float target_scale) {
  int width_px = target_width_dp * target_scale;
  return gfx::ImageSkiaRep(
      skia::ImageOperations::Resize(rep.sk_bitmap(),
                                    skia::ImageOperations::RESIZE_BEST,
                                    width_px, width_px),
      target_scale);
}

bool IsTouchOptimized() {
  return ui::MaterialDesignController::IsTouchOptimizedUiEnabled();
}

}  // namespace

IconWithBadgeImageSource::Badge::Badge(const std::string& text,
                                       SkColor text_color,
                                       SkColor background_color)
    : text(text), text_color(text_color), background_color(background_color) {}

IconWithBadgeImageSource::Badge::~Badge() {}

IconWithBadgeImageSource::IconWithBadgeImageSource(const gfx::Size& size)
    : gfx::CanvasImageSource(size, false),
      grayscale_(false),
      paint_page_action_decoration_(false),
      paint_blocked_actions_decoration_(false) {}

IconWithBadgeImageSource::~IconWithBadgeImageSource() {}

void IconWithBadgeImageSource::SetIcon(const gfx::Image& icon) {
  icon_ = icon;
}

void IconWithBadgeImageSource::SetBadge(std::unique_ptr<Badge> badge) {
  badge_ = std::move(badge);
}

void IconWithBadgeImageSource::Draw(gfx::Canvas* canvas) {
  if (icon_.IsEmpty())
    return;

  gfx::ImageSkia skia = icon_.AsImageSkia();
  gfx::ImageSkiaRep rep = skia.GetRepresentation(canvas->image_scale());
  if (rep.scale() != canvas->image_scale()) {
    skia.AddRepresentation(ScaleImageSkiaRep(
        rep, ExtensionAction::ActionIconSize(), canvas->image_scale()));
  }
  if (grayscale_)
    skia = gfx::ImageSkiaOperations::CreateHSLShiftedImage(skia, {-1, 0, 0.75});

  int x_offset =
      std::floor((size().width() - ExtensionAction::ActionIconSize()) / 2.0);
  int y_offset =
      std::floor((size().height() - ExtensionAction::ActionIconSize()) / 2.0);
  canvas->DrawImageInt(skia, x_offset, y_offset);

  // Draw a badge on the provided browser action icon's canvas.
  PaintBadge(canvas);

  if (paint_page_action_decoration_)
    PaintPageActionDecoration(canvas);

  if (paint_blocked_actions_decoration_)
    PaintBlockedActionDecoration(canvas);
}

// Paints badge with specified parameters to |canvas|.
void IconWithBadgeImageSource::PaintBadge(gfx::Canvas* canvas) {
  if (!badge_ || badge_->text.empty())
    return;

  SkColor text_color = SkColorGetA(badge_->text_color) == SK_AlphaTRANSPARENT
                           ? SK_ColorWHITE
                           : badge_->text_color;

  // Make sure the background color is opaque. See http://crbug.com/619499
  SkColor background_color =
      SkColorGetA(badge_->background_color) == SK_AlphaTRANSPARENT
          ? gfx::kGoogleBlue500
          : SkColorSetA(badge_->background_color, SK_AlphaOPAQUE);

  const int badge_height = IsTouchOptimized() ? 12 : 11;
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  gfx::FontList base_font = rb->GetFontList(ui::ResourceBundle::BaseFont)
                                .DeriveWithHeightUpperBound(badge_height);
  base::string16 utf16_text = base::UTF8ToUTF16(badge_->text);

  // See if we can squeeze a slightly larger font into the badge given the
  // actual string that is to be displayed.
  constexpr int kMaxIncrementAttempts = 5;
  for (size_t i = 0; i < kMaxIncrementAttempts; ++i) {
    int w = 0;
    int h = 0;
    gfx::FontList bigger_font =
        base_font.Derive(1, 0, gfx::Font::Weight::NORMAL);
    gfx::Canvas::SizeStringInt(utf16_text, bigger_font, &w, &h, 0,
                               gfx::Canvas::NO_ELLIPSIS);
    if (h > badge_height)
      break;
    base_font = bigger_font;
  }

  constexpr int kMaxTextWidth = 23;
  const int text_width =
        std::min(kMaxTextWidth, canvas->GetStringWidth(utf16_text, base_font));
  // Calculate badge size. It is clamped to a min width just because it looks
  // silly if it is too skinny.
  constexpr int kPadding = 2;
  int badge_width = text_width + kPadding * 2;

  const gfx::Rect icon_area = GetIconAreaRect();

  // Force the pixel width of badge to be either odd (if the icon width is odd)
  // or even otherwise. If there is a mismatch you get http://crbug.com/26400.
  if (icon_area.width() != 0 && (badge_width % 2 != icon_area.width() % 2))
    badge_width += 1;
  badge_width = std::max(badge_height, badge_width);

  // The minimum width for center-aligning the badge.
  constexpr int kCenterAlignThreshold = 20;
  // Calculate the badge background rect. It is usually right-aligned, but it
  // can also be center-aligned if it is large.
  const int badge_offset_x = badge_width >= kCenterAlignThreshold
                                 ? (icon_area.width() - badge_width) / 2
                                 : icon_area.width() - badge_width;
  const int badge_offset_y = icon_area.height() - badge_height;
  gfx::Rect rect(icon_area.x() + badge_offset_x, icon_area.y() + badge_offset_y,
                 badge_width, badge_height);
  cc::PaintFlags rect_flags;
  rect_flags.setStyle(cc::PaintFlags::kFill_Style);
  rect_flags.setAntiAlias(true);
  rect_flags.setColor(background_color);

  // Clear part of the background icon.
  gfx::Rect cutout_rect(rect);
  cutout_rect.Inset(-1, -1);
  cc::PaintFlags cutout_flags = rect_flags;
  cutout_flags.setBlendMode(SkBlendMode::kClear);
  const int outer_corner_radius = IsTouchOptimized() ? 3 : 2;
  canvas->DrawRoundRect(cutout_rect, outer_corner_radius, cutout_flags);

  // Paint the backdrop.
  const int inner_corner_radius = outer_corner_radius - 1;
  canvas->DrawRoundRect(rect, inner_corner_radius, rect_flags);

  // Paint the text.
  rect.Inset(std::max(kPadding, (rect.width() - text_width) / 2),
             badge_height - base_font.GetHeight(), kPadding, 0);
  canvas->DrawStringRect(utf16_text, base_font, text_color, rect);
}

void IconWithBadgeImageSource::PaintPageActionDecoration(gfx::Canvas* canvas) {
  static const SkColor decoration_color = SkColorSetARGB(255, 70, 142, 226);

  const gfx::Rect icon_area = GetIconAreaRect();
  const int major_radius = std::ceil(icon_area.width() / 5.0);
  const int minor_radius =
      IsTouchOptimized() ? major_radius - 1 : std::ceil(major_radius / 2.0);
  // This decoration is positioned at the bottom left corner of the icon area.
  gfx::Point center_point = icon_area.bottom_left();
  center_point.Offset(major_radius + 1, -major_radius - 1);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(SK_ColorTRANSPARENT);
  flags.setBlendMode(SkBlendMode::kSrc);
  canvas->DrawCircle(center_point, major_radius, flags);
  flags.setColor(decoration_color);
  canvas->DrawCircle(center_point, minor_radius, flags);
}

void IconWithBadgeImageSource::PaintBlockedActionDecoration(
    gfx::Canvas* canvas) {
  canvas->Save();
  gfx::ImageSkia img =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_BLOCKED_EXTENSION_SCRIPT);
  // This decoration is positioned at the top right corner of the icon area.
  const gfx::Point top_right = GetIconAreaRect().top_right();
  canvas->DrawImageInt(img, top_right.x() - img.width(), top_right.y());
  canvas->Restore();
}

gfx::Rect IconWithBadgeImageSource::GetIconAreaRect() const {
  gfx::Rect icon_area(size());
  icon_area.Inset(GetLayoutInsets(TOOLBAR_ACTION_VIEW));
  return icon_area;
}
