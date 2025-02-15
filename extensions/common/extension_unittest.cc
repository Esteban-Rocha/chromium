// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension.h"

#include "base/command_line.h"
#include "base/optional.h"
#include "base/test/scoped_command_line.h"
#include "extensions/common/switches.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

testing::AssertionResult RunManifestVersionSuccess(
    std::unique_ptr<base::DictionaryValue> manifest,
    Manifest::Type expected_type,
    int expected_manifest_version,
    Extension::InitFromValueFlags custom_flag = Extension::NO_FLAGS) {
  std::string error;
  scoped_refptr<const Extension> extension = Extension::Create(
      base::FilePath(), Manifest::INTERNAL, *manifest, custom_flag, &error);
  if (!extension) {
    return testing::AssertionFailure()
           << "Extension creation failed: " << error;
  }

  if (extension->GetType() != expected_type) {
    return testing::AssertionFailure()
           << "Wrong type: " << extension->GetType();
  }

  if (extension->manifest_version() != expected_manifest_version) {
    return testing::AssertionFailure()
           << "Wrong manifest version: " << extension->manifest_version();
  }

  return testing::AssertionSuccess();
}

testing::AssertionResult RunManifestVersionFailure(
    std::unique_ptr<base::DictionaryValue> manifest,
    Extension::InitFromValueFlags custom_flag = Extension::NO_FLAGS) {
  std::string error;
  scoped_refptr<const Extension> extension = Extension::Create(
      base::FilePath(), Manifest::INTERNAL, *manifest, custom_flag, &error);
  if (extension)
    return testing::AssertionFailure() << "Extension creation succeeded.";

  return testing::AssertionSuccess();
}

}  // namespace

// TODO(devlin): Move tests from chrome/common/extensions/extension_unittest.cc
// that don't depend on //chrome into here.

TEST(ExtensionTest, ExtensionManifestVersions) {
  auto get_manifest = [](base::Optional<int> manifest_version) {
    DictionaryBuilder builder;
    builder.Set("name", "My Extension")
        .Set("version", "0.1")
        .Set("description", "An awesome extension");
    if (manifest_version)
      builder.Set("manifest_version", *manifest_version);
    return builder.Build();
  };

  const Manifest::Type kType = Manifest::TYPE_EXTENSION;
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(2), kType, 2));
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(3), kType, 3));

  // Manifest v1 is deprecated, and should not load.
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(1)));
  // Omitting the key defaults to v1 for extensions.
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(base::nullopt)));

  // '0' and '-1' are invalid values.
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(0)));
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(-1)));

  {
    // Manifest v1 should only load if a command line switch is used....
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kAllowLegacyExtensionManifests);
    EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(1), kType, 1));
    EXPECT_TRUE(
        RunManifestVersionSuccess(get_manifest(base::nullopt), kType, 1));
  }

  {
    // ...or a runtime flag is set.
    Extension::ScopedAllowLegacyExtensions allow_legacy_extensions =
        Extension::allow_legacy_extensions_for_testing();
    EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(1), kType, 1));
    EXPECT_TRUE(
        RunManifestVersionSuccess(get_manifest(base::nullopt), kType, 1));
  }
}

TEST(ExtensionTest, PlatformAppManifestVersions) {
  auto get_manifest = [](base::Optional<int> manifest_version) {
    DictionaryBuilder background;
    background.Set("scripts", ListBuilder().Append("background.js").Build());
    DictionaryBuilder builder;
    builder.Set("name", "My Platform App")
        .Set("version", "0.1")
        .Set("description", "A platform app")
        .Set("app",
             DictionaryBuilder().Set("background", background.Build()).Build());
    if (manifest_version)
      builder.Set("manifest_version", *manifest_version);
    return builder.Build();
  };

  const Manifest::Type kType = Manifest::TYPE_PLATFORM_APP;
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(2), kType, 2));
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(3), kType, 3));
  // Omitting the key defaults to v2 for platform apps.
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(base::nullopt), kType, 2));

  // Manifest v1 is deprecated, and should not load.
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(1)));

  // '0' and '-1' are invalid values.
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(0)));
  EXPECT_TRUE(RunManifestVersionFailure(get_manifest(-1)));

  {
    // Manifest v1 should not load for platform apps, even with the command line
    // switch.
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kAllowLegacyExtensionManifests);
    EXPECT_TRUE(RunManifestVersionFailure(get_manifest(1)));
  }
}

TEST(ExtensionTest, HostedAppManifestVersions) {
  auto get_manifest = [](base::Optional<int> manifest_version) {
    DictionaryBuilder builder;
    DictionaryBuilder app;
    app.Set("urls", ListBuilder().Append("http://example.com").Build());
    builder.Set("name", "My Hosted App")
        .Set("version", "0.1")
        .Set("description", "A hosted app")
        .Set("app", app.Build());
    if (manifest_version)
      builder.Set("manifest_version", *manifest_version);
    return builder.Build();
  };

  const Manifest::Type kType = Manifest::TYPE_HOSTED_APP;
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(2), kType, 2));
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(3), kType, 3));

  // Manifest v1 is deprecated, but should still load for hosted apps.
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(1), kType, 1));
  // Omitting the key defaults to v1 for hosted apps, and v1 is still allowed.
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(base::nullopt), kType, 1));

  // Requiring the modern manifest version should make hosted apps require v2.
  EXPECT_TRUE(RunManifestVersionFailure(
      get_manifest(1), Extension::REQUIRE_MODERN_MANIFEST_VERSION));
}

TEST(ExtensionTest, UserScriptManifestVersions) {
  auto get_manifest = [](base::Optional<int> manifest_version) {
    DictionaryBuilder builder;
    builder.Set("name", "My Extension")
        .Set("version", "0.1")
        .Set("description", "An awesome extension")
        .Set("converted_from_user_script", true);
    if (manifest_version)
      builder.Set("manifest_version", *manifest_version);
    return builder.Build();
  };

  const Manifest::Type kType = Manifest::TYPE_USER_SCRIPT;
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(2), kType, 2));
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(3), kType, 3));

  // Manifest v1 is deprecated, but should still load for user scripts.
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(1), kType, 1));
  // Omitting the key defaults to v1 for user scripts, but v1 is still allowed.
  EXPECT_TRUE(RunManifestVersionSuccess(get_manifest(base::nullopt), kType, 1));

  // Requiring the modern manifest version should make user scripts require v2.
  EXPECT_TRUE(RunManifestVersionFailure(
      get_manifest(1), Extension::REQUIRE_MODERN_MANIFEST_VERSION));
}

}  // namespace extensions
