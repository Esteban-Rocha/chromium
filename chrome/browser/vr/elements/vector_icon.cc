// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/vector_icon.h"

#include <memory>

#include "chrome/browser/vr/elements/ui_texture.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

class VectorIconTexture : public UiTexture {
 public:
  VectorIconTexture() {}
  ~VectorIconTexture() override {}

  void SetColor(SkColor color) { SetAndDirty(&color_, color); }

  SkColor GetColor() const { return color_; }

  void SetIcon(const gfx::VectorIcon* icon) {
    SetAndDirty(&icon_no_1x_.rep, icon ? icon->rep : nullptr);
  }

 private:
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override {
    if (icon_no_1x_.is_empty())
      return;
    cc::SkiaPaintCanvas paint_canvas(sk_canvas);
    gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);

    size_.set_height(texture_size.height());
    size_.set_width(texture_size.width());

    float icon_size = size_.height();
    float icon_corner_offset = (size_.height() - icon_size) / 2;
    VectorIcon::DrawVectorIcon(
        &gfx_canvas, icon_no_1x_, icon_size,
        gfx::PointF(icon_corner_offset, icon_corner_offset), color_);
  }

  gfx::SizeF size_;
  gfx::VectorIcon icon_no_1x_{};
  SkColor color_ = SK_ColorWHITE;
  DISALLOW_COPY_AND_ASSIGN(VectorIconTexture);
};

VectorIcon::VectorIcon(int texture_width)
    : texture_(std::make_unique<VectorIconTexture>()),
      texture_width_(texture_width) {}
VectorIcon::~VectorIcon() {}

void VectorIcon::SetColor(SkColor color) {
  texture_->SetColor(color);
}

SkColor VectorIcon::GetColor() const {
  return texture_->GetColor();
}

void VectorIcon::SetIcon(const gfx::VectorIcon& icon) {
  texture_->SetIcon(&icon);
}

void VectorIcon::SetIcon(const gfx::VectorIcon* icon) {
  texture_->SetIcon(icon);
}

UiTexture* VectorIcon::GetTexture() const {
  return texture_.get();
}

gfx::Size VectorIcon::MeasureTextureSize() {
  return gfx::Size(texture_width_, texture_width_);
}

void VectorIcon::DrawVectorIcon(gfx::Canvas* canvas,
                                const gfx::VectorIcon& icon,
                                float size_px,
                                const gfx::PointF& corner,
                                SkColor color) {
  gfx::ScopedCanvas scoped(canvas);
  canvas->Translate(
      {static_cast<int>(corner.x()), static_cast<int>(corner.y())});

  // Explicitly cut out the 1x version of the icon, as PaintVectorIcon draws the
  // 1x version if device scale factor isn't set. See crbug.com/749146. If all
  // icons end up being drawn via VectorIcon instances, this will not be
  // required (the 1x version is automatically elided by this class).
  gfx::VectorIcon icon_no_1x{icon.rep};
  PaintVectorIcon(canvas, icon_no_1x, size_px, color);
}

}  // namespace vr
