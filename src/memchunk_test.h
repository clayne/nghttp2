/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
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
#ifndef MEMCHUNK_TEST_H
#define MEMCHUNK_TEST_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // HAVE_CONFIG_H

#define MUNIT_ENABLE_ASSERT_ALIASES

#include "munit.h"

namespace nghttp2 {

extern const MunitSuite memchunk_suite;

munit_void_test_decl(test_pool_recycle)
munit_void_test_decl(test_memchunks_append)
munit_void_test_decl(test_memchunks_drain)
munit_void_test_decl(test_memchunks_riovec)
munit_void_test_decl(test_memchunks_recycle)
munit_void_test_decl(test_memchunks_reset)
munit_void_test_decl(test_memchunkbuffer_drain_reset)

} // namespace nghttp2

#endif // MEMCHUNK_TEST_H
