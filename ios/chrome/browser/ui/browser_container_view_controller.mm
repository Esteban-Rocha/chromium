// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container_view_controller.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/util/named_guide.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserContainerViewController () {
  // Weak reference to content view, so old _contentView can be removed from
  // superview when new one is added.
  __weak UIView* _contentView;
}
@end

@implementation BrowserContainerViewController

- (void)dealloc {
  DCHECK(![_contentView superview] || [_contentView superview] == self.view);
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
}

- (void)displayContentView:(UIView*)contentView {
  if (_contentView == contentView) {
    // When voice search is launched from the NTP, the voice search button
    // NamedGuide is constrained to a subview of |_contentView|.  If this
    // occurs, removing |_contentView| from the hierarchy below will deactivate
    // the constraints.  The check against the voice search button guide is a
    // temporary workaround in order to minimize unintended side effects from
    // this change; this function should be a no-op when |_contentView| ==
    // |contentView|, regardless of the NamedGuide constraints.
    // TODO(crbug.com/826093): Remove NamedGuide check and simply early return.
    NamedGuide* voiceSearchGuide =
        [NamedGuide guideWithName:kVoiceSearchButtonGuide view:self.view];
    UIView* voiceSearchButton = voiceSearchGuide.constrainedView;
    if ([voiceSearchButton isDescendantOfView:_contentView])
      return;
  }

  DCHECK(![_contentView superview] || [_contentView superview] == self.view);
  [_contentView removeFromSuperview];
  _contentView = contentView;

  if (contentView)
    [self.view addSubview:contentView];
}

@end
