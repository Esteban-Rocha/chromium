// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"

namespace mojo {

// static
bool StructTraits<skia::mojom::BitmapDataView, SkBitmap>::IsNull(
    const SkBitmap& b) {
  return b.isNull();
}

// static
void StructTraits<skia::mojom::BitmapDataView, SkBitmap>::SetToNull(
    SkBitmap* b) {
  b->reset();
}

// static
const SkImageInfo& StructTraits<skia::mojom::BitmapDataView,
                                SkBitmap>::image_info(const SkBitmap& b) {
  return b.info();
}

// static
uint64_t StructTraits<skia::mojom::BitmapDataView, SkBitmap>::row_bytes(
    const SkBitmap& b) {
  return b.rowBytes();
}

// static
BitmapBuffer StructTraits<skia::mojom::BitmapDataView, SkBitmap>::pixel_data(
    const SkBitmap& b) {
  return BitmapBuffer(static_cast<uint8_t*>(b.getPixels()),
                      b.computeByteSize());
}

// static
bool StructTraits<skia::mojom::BitmapDataView, SkBitmap>::Read(
    skia::mojom::BitmapDataView data,
    SkBitmap* b) {
  // TODO: Ensure width and height are reasonable, eg. <= kMaxBitmapSize?
  SkImageInfo image_info;
  if (!data.ReadImageInfo(&image_info))
    return false;

  *b = SkBitmap();
  if (!b->tryAllocPixels(image_info, data.row_bytes())) {
    return false;
  }

  // If the image is empty, return success after setting the image info.
  if (image_info.width() == 0 || image_info.height() == 0)
    return true;

  mojo::ArrayDataView<uint8_t> data_view;
  data.GetPixelDataDataView(&data_view);
  if (static_cast<int>(b->width()) != image_info.width() ||
      static_cast<int>(b->height()) != image_info.height() ||
      static_cast<uint64_t>(b->rowBytes()) != data.row_bytes() ||
      b->computeByteSize() != data_view.size() || !b->readyToDraw()) {
    return false;
  }

  BitmapBuffer bitmap_buffer(static_cast<uint8_t*>(b->getPixels()),
                             b->computeByteSize());
  if (!data.ReadPixelData(&bitmap_buffer) ||
      bitmap_buffer.size() != b->computeByteSize())
    return false;

  b->notifyPixelsChanged();
  return true;
}

}  // namespace mojo
