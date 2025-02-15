// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_WORKER_FETCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_WORKER_FETCH_CONTEXT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_application_cache_host.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/platform/web_socket_handshake_throttle.h"
#include "third_party/blink/public/platform/web_url.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace blink {

class WebURLLoaderFactory;
class WebURLRequest;
class WebDocumentSubresourceFilter;

// WebWorkerFetchContext is a per-worker object created on the main thread,
// passed to a worker (dedicated, shared and service worker) and initialized on
// the worker thread by InitializeOnWorkerThread(). It contains information
// about the resource fetching context (ex: service worker provider id), and is
// used to create a new WebURLLoader instance in the worker thread.
class WebWorkerFetchContext {
 public:
  virtual ~WebWorkerFetchContext() = default;

  // Set a raw pointer of a WaitableEvent which will be signaled from the main
  // thread when the worker's GlobalScope is terminated, which will terminate
  // sync loading requests on the worker thread. It is guaranteed that the
  // pointer is valid throughout the lifetime of this context.
  virtual void SetTerminateSyncLoadEvent(base::WaitableEvent*) = 0;

  virtual void InitializeOnWorkerThread() = 0;

  // Returns a new WebURLLoaderFactory which is associated with the worker
  // context. It can be called only once.
  virtual std::unique_ptr<WebURLLoaderFactory> CreateURLLoaderFactory() = 0;

  // Returns a new WebURLLoaderFactory that wraps the given
  // network::mojom::URLLoaderFactory.
  virtual std::unique_ptr<WebURLLoaderFactory> WrapURLLoaderFactory(
      mojo::ScopedMessagePipeHandle url_loader_factory_handle) = 0;

  // Called when a request is about to be sent out to modify the request to
  // handle the request correctly in the loading stack later. (Example: service
  // worker)
  virtual void WillSendRequest(WebURLRequest&) = 0;

  // Whether the fetch context is controlled by a service worker.
  virtual bool IsControlledByServiceWorker() const = 0;

  // This flag is used to block all mixed content in subframes.
  virtual void SetIsOnSubframe(bool) {}
  virtual bool IsOnSubframe() const { return false; }

  // The URL that should be consulted for the third-party cookie blocking
  // policy, as defined in Section 2.1.1 and 2.1.2 of
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site.
  // See content::URLRequest::site_for_cookies() for details.
  virtual WebURL SiteForCookies() const = 0;

  // Reports the certificate error to the browser process.
  virtual void DidRunContentWithCertificateErrors() {}
  virtual void DidDisplayContentWithCertificateErrors() {}

  // Reports that the security origin has run active content from an insecure
  // source.
  virtual void DidRunInsecureContent(const WebSecurityOrigin&,
                                     const WebURL& insecure_url) {}

  virtual void SetApplicationCacheHostID(int id) {}
  virtual int ApplicationCacheHostID() const {
    return WebApplicationCacheHost::kAppCacheNoHostId;
  }

  // Sets the builder object of WebDocumentSubresourceFilter on the main thread
  // which will be used in TakeSubresourceFilter() to create a
  // WebDocumentSubresourceFilter on the worker thread.
  virtual void SetSubresourceFilterBuilder(
      std::unique_ptr<WebDocumentSubresourceFilter::Builder>) {}

  // Creates a WebDocumentSubresourceFilter on the worker thread using the
  // WebDocumentSubresourceFilter::Builder which is set on the main thread.
  // This method should only be called once.
  virtual std::unique_ptr<WebDocumentSubresourceFilter>
  TakeSubresourceFilter() {
    return nullptr;
  }

  // Creates a WebSocketHandshakeThrottle on the worker thread.
  virtual std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle() {
    return nullptr;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_WORKER_FETCH_CONTEXT_H_
