// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.webkit.ServiceWorkerWebSettings;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;

import com.android.webview.chromium.WebkitToSharedGlueConverter;

import org.chromium.support_lib_boundary.WebkitToCompatConverterBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;

import java.lang.reflect.InvocationHandler;

/**
 * Adapter used for fetching implementations for Compat objects given their corresponding
 * webkit-object.
 */
class SupportLibWebkitToCompatConverterAdapter implements WebkitToCompatConverterBoundaryInterface {
    SupportLibWebkitToCompatConverterAdapter() {}

    // WebSettingsBoundaryInterface
    @Override
    public InvocationHandler convertSettings(WebSettings webSettings) {
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibWebSettingsAdapter(
                        WebkitToSharedGlueConverter.getSettings(webSettings)));
    }

    // ServiceWorkerWebSettingsBoundaryInterface
    @Override
    public InvocationHandler convertServiceWorkerSettings(
            /* ServiceWorkerWebSettings */ Object serviceWorkerWebSettings) {
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibServiceWorkerSettingsAdapter(
                        WebkitToSharedGlueConverter.getServiceWorkerSettings(
                                (ServiceWorkerWebSettings) serviceWorkerWebSettings)));
    }

    // WebResourceRequestBoundaryInterface
    @Override
    public InvocationHandler convertWebResourceRequest(WebResourceRequest request) {
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibWebResourceRequest(
                        WebkitToSharedGlueConverter.getWebResourceRequest(request)));
    }
}
