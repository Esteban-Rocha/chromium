// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestions_fetch.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/ntp_snippets/contextual/contextual_suggestion.h"
#include "components/ntp_snippets/contextual/proto/chrome_search_api_request_context.pb.h"
#include "components/ntp_snippets/contextual/proto/get_pivots_request.pb.h"
#include "components/ntp_snippets/contextual/proto/get_pivots_response.pb.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"

using base::Base64Encode;

namespace contextual_suggestions {

namespace {

static constexpr char kFetchEndpoint[] =
    "https://www.google.com/httpservice/web/ExploreService/GetPivots/";

static constexpr int kNumberOfSuggestionsToFetch = 10;
static constexpr int kMinNumberOfClusters = 1;
static constexpr int kMaxNumberOfClusters = 5;

ContextualSuggestion ItemToSuggestion(const PivotItem& item) {
  PivotDocument document = item.document();

  return SuggestionBuilder(GURL(document.url().raw_url()))
      .Title(document.title())
      .Snippet(document.summary())
      .PublisherName(document.site_name())
      .ImageId(document.image().id().encrypted_docid())
      .FaviconImageId(document.favicon_image().id().encrypted_docid())
      .Build();
}

Cluster PivotToCluster(const PivotCluster& pivot) {
  ClusterBuilder cluster_builder(pivot.label().label());
  for (PivotItem item : pivot.item()) {
    cluster_builder.AddSuggestion(ItemToSuggestion(item));
  }

  return cluster_builder.Build();
}

std::vector<Cluster> ClustersFromResponse(
    const GetPivotsResponse& response_proto) {
  std::vector<Cluster> clusters;
  Pivots pivots = response_proto.pivots();

  if (pivots.item_size() > 0) {
    // If the first item is a cluster, we can assume they all are.
    if (pivots.item()[0].has_cluster()) {
      clusters.reserve(response_proto.pivots().item_size());
      for (auto item : response_proto.pivots().item()) {
        if (item.has_cluster()) {
          PivotCluster pivot_cluster = item.cluster();
          clusters.emplace_back(PivotToCluster(pivot_cluster));
        }
      }
    } else if (pivots.item()[0].has_document()) {
      Cluster single_cluster;
      for (auto item : pivots.item()) {
        single_cluster.suggestions.emplace_back(ItemToSuggestion(item));
      }
      clusters.emplace_back(std::move(single_cluster));
    }
  }

  return clusters;
}

std::string PeekTextFromResponse(const GetPivotsResponse& response_proto) {
  return response_proto.pivots().peek_text().text();
}

const std::string SerializedPivotsRequest(const std::string& url,
                                          const std::string& bcp_language) {
  GetPivotsRequest pivot_request;

  SearchApiRequestContext* search_context = pivot_request.mutable_context();
  search_context->mutable_localization_context()->set_spoken_language(
      bcp_language);

  GetPivotsQuery* query = pivot_request.mutable_query();
  query->add_context()->set_url(url);

  PivotDocumentParams* params = query->mutable_document_params();
  params->set_enabled(true);
  params->set_num(kNumberOfSuggestionsToFetch);
  params->set_enable_images(true);
  params->set_image_aspect_ratio(PivotDocumentParams::SQUARE);

  PivotClusteringParams* cluster_params = query->mutable_clustering_params();
  cluster_params->set_enabled(true);
  cluster_params->set_min(kMinNumberOfClusters);
  cluster_params->set_max(kMaxNumberOfClusters);

  query->mutable_peek_text_params()->set_enabled(true);

  return pivot_request.SerializeAsString();
}

}  // namespace

ContextualSuggestionsFetch::ContextualSuggestionsFetch(
    const GURL& url,
    const std::string& bcp_language_code)
    : url_(url), bcp_language_code_(bcp_language_code) {}

ContextualSuggestionsFetch::~ContextualSuggestionsFetch() = default;

// static
const std::string ContextualSuggestionsFetch::GetFetchEndpoint() {
  return kFetchEndpoint;
}

void ContextualSuggestionsFetch::Start(
    FetchClustersCallback callback,
    ReportFetchMetricsCallback metrics_callback,
    const scoped_refptr<network::SharedURLLoaderFactory>& loader_factory) {
  request_completed_callback_ = std::move(callback);
  metrics_callback.Run(FETCH_REQUESTED);
  url_loader_ = MakeURLLoader();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory.get(),
      base::BindOnce(&ContextualSuggestionsFetch::OnURLLoaderComplete,
                     base::Unretained(this), std::move(metrics_callback)));
}

std::unique_ptr<network::SimpleURLLoader>
ContextualSuggestionsFetch::MakeURLLoader() const {
  // TODO(pnoland, https://crbug.com/831693): Update this once there's an
  // opt-out setting.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ntp_contextual_suggestions_fetch",
                                          R"(
        semantics {
          sender: "Contextual Suggestions Fetch"
          description:
            "Chromium can show contextual suggestions that are related to the "
            "currently visited page."
          trigger:
            "Triggered when a page is visited and stayed on for more than a "
            "few seconds."
          data:
            "The URL of the current tab and the ui language."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be disabled by the flag "
            "enable-contextual-suggestions-bottom-sheet."
          policy_exception_justification: "Not implemented. The feature is "
          "currently Android-only and disabled for all enterprise users. "
          "A policy will be added before enabling for enterprise users."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url = GURL(GetFetchEndpoint());
  // TODO(pnoland): include cookies once the suggestions endpoint can make use
  // of them.
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DO_NOT_SAVE_COOKIES |
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SEND_AUTH_DATA;
  resource_request->headers = MakeHeaders();
  resource_request->method = "GET";

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_loader->SetAllowHttpErrorResults(true);
  return simple_loader;
}

net::HttpRequestHeaders ContextualSuggestionsFetch::MakeHeaders() const {
  net::HttpRequestHeaders headers;
  std::string serialized_request_body =
      SerializedPivotsRequest(url_.spec(), bcp_language_code_);
  std::string base64_encoded_body;
  base::Base64UrlEncode(serialized_request_body,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &base64_encoded_body);
  headers.SetHeader("X-Protobuffer-Request-Payload", base64_encoded_body);
  variations::AppendVariationHeaders(url_, variations::InIncognito::kNo,
                                     variations::SignedIn::kNo, &headers);

  UMA_HISTOGRAM_COUNTS_1M(
      "ContextualSuggestions.FetchRequestProtoSizeKB",
      static_cast<int>(base64_encoded_body.length() / 1024));

  return headers;
}

void ContextualSuggestionsFetch::OnURLLoaderComplete(
    ReportFetchMetricsCallback metrics_callback,
    std::unique_ptr<std::string> result) {
  std::vector<Cluster> clusters;
  std::string peek_text;

  int32_t response_code = 0;
  int32_t error_code = url_loader_->NetError();
  if (result) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();

    if (response_code == net::HTTP_OK) {
      // The response comes in the format (length, bytes) where length is a
      // varint32 encoded int. Rather than hand-rolling logic to skip the
      // length(which we don't actually need), we use EncodedInputStream to
      // skip past it, then parse the remainder of the stream.
      google::protobuf::io::CodedInputStream coded_stream(
          reinterpret_cast<const uint8_t*>(result->data()), result->length());
      uint32_t response_size;
      if (coded_stream.ReadVarint32(&response_size) && response_size != 0) {
        GetPivotsResponse response_proto;
        if (response_proto.MergePartialFromCodedStream(&coded_stream)) {
          clusters = ClustersFromResponse(response_proto);
          peek_text = PeekTextFromResponse(response_proto);
        }
      }
    }

    UMA_HISTOGRAM_COUNTS_1M("ContextualSuggestions.FetchResponseSizeKB",
                            static_cast<int>(result->length() / 1024));
  }

  ReportFetchMetrics(error_code, response_code, clusters.size(),
                     std::move(metrics_callback));

  std::move(request_completed_callback_).Run(peek_text, std::move(clusters));
}

void ContextualSuggestionsFetch::ReportFetchMetrics(
    int32_t error_code,
    int32_t response_code,
    size_t clusters_size,
    ReportFetchMetricsCallback metrics_callback) {
  ContextualSuggestionsEvent event;
  if (error_code != net::OK) {
    event = FETCH_ERROR;
  } else if (response_code == net::HTTP_SERVICE_UNAVAILABLE) {
    event = FETCH_SERVER_BUSY;
  } else if (response_code != net::HTTP_OK) {
    event = FETCH_ERROR;
  } else if (clusters_size == 0) {
    event = FETCH_EMPTY;
  } else {
    event = FETCH_COMPLETED;
  }

  base::UmaHistogramSparse("ContextualSuggestions.FetchErrorCode", error_code);
  if (response_code > 0) {
    base::UmaHistogramSparse("ContextualSuggestions.FetchResponseCode",
                             response_code);
  }

  std::move(metrics_callback).Run(event);
}

}  // namespace contextual_suggestions