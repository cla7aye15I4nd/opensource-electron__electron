// Copyright (c) 2026 Microsoft Corporation.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_COMMON_GIN_HELPER_PER_ISOLATE_TEMPLATE_DATA_H_
#define ELECTRON_SHELL_COMMON_GIN_HELPER_PER_ISOLATE_TEMPLATE_DATA_H_

#include "base/memory/raw_ptr.h"
#include "gin/per_isolate_data.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace gin_helper {

// Caches v8::FunctionTemplates per isolate, keyed by an arbitrary pointer such
// as a gin::WrapperInfo, so that every context in an isolate instantiates the
// same template. gin::PerIsolateData caches ObjectTemplates this way but no
// longer caches FunctionTemplates. The cache is released when the isolate is
// disposed.
class PerIsolateTemplateData : public gin::PerIsolateData::DisposeObserver {
 public:
  // Returns the template cached for `key` in `isolate`, or an empty handle if
  // there is none or the isolate has no gin::PerIsolateData.
  static v8::Local<v8::FunctionTemplate> GetFunctionTemplate(
      v8::Isolate* isolate,
      const void* key);

  // Caches `templ` for `key` in `isolate`. Does nothing if the isolate has no
  // gin::PerIsolateData.
  static void SetFunctionTemplate(v8::Isolate* isolate,
                                  const void* key,
                                  v8::Local<v8::FunctionTemplate> templ);

  PerIsolateTemplateData(const PerIsolateTemplateData&) = delete;
  PerIsolateTemplateData& operator=(const PerIsolateTemplateData&) = delete;
  ~PerIsolateTemplateData() override;

 private:
  explicit PerIsolateTemplateData(gin::PerIsolateData* per_isolate_data);

  // Returns the cache of `isolate`, creating it on first use, or nullptr if the
  // isolate has no gin::PerIsolateData.
  static PerIsolateTemplateData* From(v8::Isolate* isolate);

  // gin::PerIsolateData::DisposeObserver:
  void OnBeforeDispose(v8::Isolate* isolate) override;
  void OnDisposed() override;

  const raw_ptr<gin::PerIsolateData> per_isolate_data_;
  absl::flat_hash_map<const void*, v8::Global<v8::FunctionTemplate>> templates_;
};

}  // namespace gin_helper

#endif  // ELECTRON_SHELL_COMMON_GIN_HELPER_PER_ISOLATE_TEMPLATE_DATA_H_
