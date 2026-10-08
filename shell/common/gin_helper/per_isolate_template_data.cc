// Copyright (c) 2026 Microsoft Corporation.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/common/gin_helper/per_isolate_template_data.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-template.h"

namespace gin_helper {

namespace {

// The caches of all live isolates. Each isolate only touches its own cache on
// its own thread, but isolates are created and disposed on different threads.
class Registry {
 public:
  static Registry& Get() {
    static base::NoDestructor<Registry> instance;
    return *instance;
  }

  PerIsolateTemplateData* Find(v8::Isolate* isolate) {
    base::AutoLock lock(lock_);
    auto it = caches_.find(isolate);
    return it == caches_.end() ? nullptr : it->second.get();
  }

  PerIsolateTemplateData* Add(v8::Isolate* isolate,
                              std::unique_ptr<PerIsolateTemplateData> cache) {
    base::AutoLock lock(lock_);
    return caches_.insert_or_assign(isolate, std::move(cache))
        .first->second.get();
  }

  std::unique_ptr<PerIsolateTemplateData> Take(v8::Isolate* isolate) {
    base::AutoLock lock(lock_);
    auto node = caches_.extract(isolate);
    return node.empty() ? nullptr : std::move(node.mapped());
  }

 private:
  base::Lock lock_;
  absl::flat_hash_map<v8::Isolate*, std::unique_ptr<PerIsolateTemplateData>>
      caches_ GUARDED_BY(lock_);
};

}  // namespace

// static
v8::Local<v8::FunctionTemplate> PerIsolateTemplateData::GetFunctionTemplate(
    v8::Isolate* isolate,
    const void* key) {
  auto* cache = From(isolate);
  if (!cache)
    return {};
  auto it = cache->templates_.find(key);
  if (it == cache->templates_.end())
    return {};
  return it->second.Get(isolate);
}

// static
void PerIsolateTemplateData::SetFunctionTemplate(
    v8::Isolate* isolate,
    const void* key,
    v8::Local<v8::FunctionTemplate> templ) {
  auto* cache = From(isolate);
  if (!cache)
    return;
  cache->templates_[key].Reset(isolate, templ);
}

PerIsolateTemplateData::PerIsolateTemplateData(
    gin::PerIsolateData* per_isolate_data)
    : per_isolate_data_(per_isolate_data) {
  per_isolate_data_->AddDisposeObserver(this);
}

PerIsolateTemplateData::~PerIsolateTemplateData() {
  per_isolate_data_->RemoveDisposeObserver(this);
}

// static
PerIsolateTemplateData* PerIsolateTemplateData::From(v8::Isolate* isolate) {
  if (auto* cache = Registry::Get().Find(isolate))
    return cache;
  auto* per_isolate_data = gin::PerIsolateData::From(isolate);
  if (!per_isolate_data)
    return nullptr;
  return Registry::Get().Add(
      isolate, base::WrapUnique(new PerIsolateTemplateData(per_isolate_data)));
}

void PerIsolateTemplateData::OnBeforeDispose(v8::Isolate* isolate) {
  // The isolate is still alive here, so the handles can be released. Dropping
  // the registry entry destroys this observer, which removes itself from
  // `per_isolate_data_`, so OnDisposed() never runs.
  templates_.clear();
  Registry::Get().Take(isolate);
}

void PerIsolateTemplateData::OnDisposed() {}

}  // namespace gin_helper
