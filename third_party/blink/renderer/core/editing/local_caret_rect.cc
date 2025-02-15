/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/local_caret_rect.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/inline_box_position.h"
#include "third_party/blink/renderer/core/editing/ng_flat_tree_shorthands.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"

namespace blink {

namespace {

// Returns true if |layout_object| and |offset| points after line end.
template <typename Strategy>
bool NeedsLineEndAdjustment(
    const PositionWithAffinityTemplate<Strategy>& adjusted) {
  const PositionTemplate<Strategy>& position = adjusted.GetPosition();
  const LayoutObject& layout_object = *position.AnchorNode()->GetLayoutObject();
  if (!layout_object.IsText())
    return false;
  const LayoutText& layout_text = ToLayoutText(layout_object);
  if (layout_text.IsBR())
    return position.IsAfterAnchor();
  // For normal text nodes.
  if (!layout_text.Style()->PreserveNewline())
    return false;
  if (!layout_text.TextLength() ||
      layout_text.CharacterAt(layout_text.TextLength() - 1) != '\n')
    return false;
  if (position.IsAfterAnchor())
    return true;
  return position.IsOffsetInAnchor() &&
         position.OffsetInContainerNode() ==
             static_cast<int>(layout_text.TextLength());
}

// Returns the first InlineBoxPosition at next line of last InlineBoxPosition
// in |layout_object| if it exists to avoid making InlineBoxPosition at end of
// line.
template <typename Strategy>
InlineBoxPosition NextLinePositionOf(
    const PositionWithAffinityTemplate<Strategy>& adjusted) {
  const PositionTemplate<Strategy>& position = adjusted.GetPosition();
  const LayoutText& layout_text =
      ToLayoutTextOrDie(*position.AnchorNode()->GetLayoutObject());
  InlineTextBox* const last = layout_text.LastTextBox();
  if (!last)
    return InlineBoxPosition();
  const RootInlineBox& root = last->Root();
  const RootInlineBox* const next_root = root.NextRootBox();
  if (!next_root)
    return InlineBoxPosition();
  InlineBox* const inline_box = next_root->FirstLeafChild();
  return AdjustInlineBoxPositionForTextDirection(
      inline_box, inline_box->CaretMinOffset(),
      layout_text.Style()->GetUnicodeBidi());
}

template <typename Strategy>
LocalCaretRect LocalCaretRectOfPositionTemplate(
    const PositionWithAffinityTemplate<Strategy>& position,
    LayoutUnit* extra_width_to_end_of_line) {
  if (position.IsNull())
    return LocalCaretRect();
  Node* const node = position.AnchorNode();
  LayoutObject* const layout_object = node->GetLayoutObject();
  if (!layout_object)
    return LocalCaretRect();

  const PositionWithAffinityTemplate<Strategy>& adjusted =
      ComputeInlineAdjustedPosition(position);

  if (adjusted.IsNotNull()) {
    if (const LayoutBlockFlow* context =
            NGInlineFormattingContextOf(adjusted.GetPosition()))
      return ComputeNGLocalCaretRect(*context, adjusted);

    // TODO(editing-dev): This DCHECK is for ensuring the correctness of
    // breaking |ComputeInlineBoxPosition| into |ComputeInlineAdjustedPosition|
    // and |ComputeInlineBoxPositionForInlineAdjustedPosition|. If there is any
    // DCHECK hit, we should pass primary direction to the latter function.
    // TODO(crbug.com/793098): Fix it so that we don't need to bother about
    // primary direction.
    DCHECK_EQ(PrimaryDirectionOf(*position.AnchorNode()),
              PrimaryDirectionOf(*adjusted.AnchorNode()));
    const InlineBoxPosition& box_position =
        NeedsLineEndAdjustment(adjusted)
            ? NextLinePositionOf(adjusted)
            : ComputeInlineBoxPositionForInlineAdjustedPosition(adjusted);

    if (box_position.inline_box) {
      const LayoutObject* box_layout_object =
          LineLayoutAPIShim::LayoutObjectFrom(
              box_position.inline_box->GetLineLayoutItem());
      return LocalCaretRect(
          box_layout_object,
          box_layout_object->LocalCaretRect(box_position.inline_box,
                                            box_position.offset_in_box,
                                            extra_width_to_end_of_line));
    }
  }

  // DeleteSelectionCommandTest.deleteListFromTable goes here.
  return LocalCaretRect(
      layout_object, layout_object->LocalCaretRect(
                         nullptr, position.GetPosition().ComputeEditingOffset(),
                         extra_width_to_end_of_line));
}

// This function was added because the caret rect that is calculated by
// using the line top value instead of the selection top.
template <typename Strategy>
LocalCaretRect LocalSelectionRectOfPositionTemplate(
    const PositionWithAffinityTemplate<Strategy>& position) {
  if (position.IsNull())
    return LocalCaretRect();
  Node* const node = position.AnchorNode();
  if (!node->GetLayoutObject())
    return LocalCaretRect();

  const PositionWithAffinityTemplate<Strategy>& adjusted =
      ComputeInlineAdjustedPosition(position);
  if (adjusted.IsNull())
    return LocalCaretRect();

  if (const LayoutBlockFlow* context =
          NGInlineFormattingContextOf(adjusted.GetPosition())) {
    // TODO(editing-dev): Use selection height instead of caret height, or
    // decide if we need to keep the distinction between caret height and
    // selection height in NG.
    return ComputeNGLocalCaretRect(*context, adjusted);
  }

  // TODO(editing-dev): This DCHECK is for ensuring the correctness of
  // breaking |ComputeInlineBoxPosition| into |ComputeInlineAdjustedPosition|
  // and |ComputeInlineBoxPositionForInlineAdjustedPosition|. If there is any
  // DCHECK hit, we should pass primary direction to the latter function.
  // TODO(crbug.com/793098): Fix it so that we don't need to bother about
  // primary direction.
  DCHECK_EQ(PrimaryDirectionOf(*position.AnchorNode()),
            PrimaryDirectionOf(*adjusted.AnchorNode()));
  const InlineBoxPosition& box_position =
      ComputeInlineBoxPositionForInlineAdjustedPosition(adjusted);

  if (!box_position.inline_box)
    return LocalCaretRect();

  LayoutObject* const layout_object = LineLayoutAPIShim::LayoutObjectFrom(
      box_position.inline_box->GetLineLayoutItem());

  const LayoutRect& rect = layout_object->LocalCaretRect(
      box_position.inline_box, box_position.offset_in_box);

  if (rect.IsEmpty())
    return LocalCaretRect();

  const InlineBox* const box = box_position.inline_box;
  if (layout_object->Style()->IsHorizontalWritingMode()) {
    return LocalCaretRect(
        layout_object,
        LayoutRect(LayoutPoint(rect.X(), box->Root().SelectionTop()),
                   LayoutSize(rect.Width(), box->Root().SelectionHeight())));
  }

  return LocalCaretRect(
      layout_object,
      LayoutRect(LayoutPoint(box->Root().SelectionTop(), rect.Y()),
                 LayoutSize(box->Root().SelectionHeight(), rect.Height())));
}

}  // namespace

LocalCaretRect LocalCaretRectOfPosition(
    const PositionWithAffinity& position,
    LayoutUnit* extra_width_to_end_of_line) {
  return LocalCaretRectOfPositionTemplate<EditingStrategy>(
      position, extra_width_to_end_of_line);
}

LocalCaretRect LocalCaretRectOfPosition(
    const PositionInFlatTreeWithAffinity& position) {
  return LocalCaretRectOfPositionTemplate<EditingInFlatTreeStrategy>(position,
                                                                     nullptr);
}

LocalCaretRect LocalSelectionRectOfPosition(
    const PositionWithAffinity& position) {
  return LocalSelectionRectOfPositionTemplate<EditingStrategy>(position);
}

// ----

template <typename Strategy>
static IntRect AbsoluteCaretBoundsOfAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  const LocalCaretRect& caret_rect =
      LocalCaretRectOfPosition(visible_position.ToPositionWithAffinity());
  if (caret_rect.IsEmpty())
    return IntRect();
  return LocalToAbsoluteQuadOf(caret_rect).EnclosingBoundingBox();
}

IntRect AbsoluteCaretBoundsOf(const VisiblePosition& visible_position) {
  return AbsoluteCaretBoundsOfAlgorithm<EditingStrategy>(visible_position);
}

// TODO(editing-dev): This function does pretty much the same thing as
// |AbsoluteCaretBoundsOf()|. Consider merging them.
IntRect AbsoluteCaretRectOfPosition(const PositionWithAffinity& position,
                                    LayoutUnit* extra_width_to_end_of_line) {
  const LocalCaretRect local_caret_rect =
      LocalCaretRectOfPosition(position, extra_width_to_end_of_line);
  if (!local_caret_rect.layout_object)
    return IntRect();

  const IntRect local_rect = PixelSnappedIntRect(local_caret_rect.rect);
  return local_rect == IntRect()
             ? IntRect()
             : local_caret_rect.layout_object
                   ->LocalToAbsoluteQuad(FloatRect(local_rect))
                   .EnclosingBoundingBox();
}

template <typename Strategy>
static IntRect AbsoluteSelectionBoundsOfAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  const LocalCaretRect& caret_rect =
      LocalSelectionRectOfPosition(visible_position.ToPositionWithAffinity());
  if (caret_rect.IsEmpty())
    return IntRect();
  return LocalToAbsoluteQuadOf(caret_rect).EnclosingBoundingBox();
}

IntRect AbsoluteSelectionBoundsOf(const VisiblePosition& visible_position) {
  return AbsoluteSelectionBoundsOfAlgorithm<EditingStrategy>(visible_position);
}

IntRect AbsoluteCaretBoundsOf(
    const VisiblePositionInFlatTree& visible_position) {
  return AbsoluteCaretBoundsOfAlgorithm<EditingInFlatTreeStrategy>(
      visible_position);
}

}  // namespace blink
