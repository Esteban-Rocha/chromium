// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;

import java.lang.reflect.InvocationHandler;

/**
 * Boundary interface for a class used for converting webkit objects into Compat (support library)
 * objects.
 */
public interface WebkitToCompatConverterBoundaryInterface {
    /* SupportLibraryWebSettings */ InvocationHandler convertSettings(WebSettings webSettings);
    /* SupportLibServiceWorkerSettings */ InvocationHandler convertServiceWorkerSettings(
            /* ServiceWorkerWebSettings */ Object serviceWorkerWebSettings);
    /* SupportLibraryWebResourceRequest */ InvocationHandler convertWebResourceRequest(
            WebResourceRequest request);
}
