/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * This is Instance::requestQfams().
 */
#include <queue>

#include "language.h"

namespace language {

QueueRequest::~QueueRequest(){};

std::vector<QueueRequest> Instance::requestQfams(
    size_t dev_i, std::set<SurfaceSupport> support) {
  // Prioritize support provided by each dev.qfams.
  struct QueueFamilySupport {
    QueueFamilySupport(size_t di, size_t qi, const std::set<SurfaceSupport>& s)
        : dev_i(di), q_i(qi), support(s) {}

    size_t dev_i;
    size_t q_i;
    std::set<SurfaceSupport> support;
  };
  struct QueueFamilySupportComparator {
    bool operator()(QueueFamilySupport a, QueueFamilySupport b) const {
      if (a.support.size() > b.support.size()) return true;
      if (a.support.size() < b.support.size()) return false;
      auto b_i = b.support.begin();
      for (auto a_i = a.support.begin(); a_i != a.support.end(); a_i++, b_i++) {
        if (*a_i < *b_i) return true;
        if (*a_i > *b_i) return false;
      }
      if (a.dev_i < b.dev_i) return true;
      if (a.dev_i > b.dev_i) return false;
      return a.q_i < b.q_i;
    }
  };
  std::set<QueueFamilySupport, QueueFamilySupportComparator> prio;
  auto& dev = *devs.at(dev_i);

  for (size_t q_i = 0; q_i < dev.qfams.size(); q_i++) {
    auto& fam = dev.qfams.at(q_i);
    std::set<SurfaceSupport> qsupport;
    for (auto s_i = support.begin(); s_i != support.end(); s_i++) {
      auto s = *s_i;
      if (s == GRAPHICS && fam.isGraphics()) {
        qsupport.emplace(GRAPHICS);
      } else if (s == COMPUTE && fam.isCompute()) {
        qsupport.emplace(COMPUTE);
      } else if (fam.surfaceSupport() == s) {
        qsupport.emplace(s);
      }
    }
    if (qsupport.size() > 0) {
      prio.emplace(dev_i, q_i, qsupport);
    }
  }

  // Fill the requested support from the priority queue.
  std::vector<QueueRequest> result;
  for (auto s_i = support.begin(); s_i != support.end();) {
    auto s = *s_i;
    auto prio_i = prio.begin();
    for (; prio_i != prio.end(); prio_i++) {
      if (prio_i->support.find(s) != prio_i->support.end()) {
        // prio_i contains s. Because prio only contains values s will have,
        // this ends the search for s.
        break;
      }
    }
    if (prio_i == prio.end()) {
      s_i++;
      continue;
    }
    result.emplace_back(prio_i->dev_i, prio_i->q_i);

    // Remove all support needs supplied by prio_i.
    auto prio_s_i = prio_i->support.begin();
    for (; prio_s_i != prio_i->support.end(); prio_s_i++) {
      size_t found = support.erase(*prio_s_i);
      if (found) {
        s_i = support.begin();
      }
    }
  }
  if (support.size() > 0) {
    logW("requestQfams: %zu queue families not found on dev[%zu]:\n",
         support.size(), dev_i);
    for (auto s_i = support.begin(); s_i != support.end(); s_i++) {
      logW("requestQfams: queue family %d not found on dev[%zu]\n", (int)*s_i,
           dev_i);
    }
    result.clear();
  }
  return result;
}

size_t Device::getQfamI(SurfaceSupport support) const {
  for (size_t i = 0; i < qfams.size(); i++) {
    auto& fam = qfams.at(i);
    if (support == GRAPHICS && fam.isGraphics()) return i;
    if (support == COMPUTE && fam.isCompute()) return i;
    if (support == fam.surfaceSupport()) return i;
  }
  logE("getQfamI(%d): not found\n", (int)support);
  return (size_t)-1;
}

}  // namespace language
