/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2014 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "shrpx_downstream_connection_pool.h"
#include "shrpx_downstream_connection.h"

namespace shrpx {

DownstreamConnectionPool::DownstreamConnectionPool() {}

DownstreamConnectionPool::~DownstreamConnectionPool() { remove_all(); }

void DownstreamConnectionPool::remove_all() {
  for (auto dconn : pool_) {
    delete dconn;
  }

  pool_.clear();
}

void DownstreamConnectionPool::add_downstream_connection(
  std::unique_ptr<DownstreamConnection> dconn) {
  pool_.insert(dconn.release());
}

std::unique_ptr<DownstreamConnection>
DownstreamConnectionPool::pop_downstream_connection() {
  if (pool_.empty()) {
    return nullptr;
  }

  auto it = std::ranges::begin(pool_);
  auto dconn = std::unique_ptr<DownstreamConnection>(*it);
  pool_.erase(it);

  return dconn;
}

void DownstreamConnectionPool::remove_downstream_connection(
  DownstreamConnection *dconn) {
  pool_.erase(dconn);
  delete dconn;
}

} // namespace shrpx
