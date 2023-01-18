/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

typedef struct {
    int8_t ndim;
    int64_t shape[CATERVA_MAX_DIM];
    int32_t chunkshape[CATERVA_MAX_DIM];
    int32_t blockshape[CATERVA_MAX_DIM];
    int32_t chunkshape2[CATERVA_MAX_DIM];
    int32_t blockshape2[CATERVA_MAX_DIM];
} test_shapes_t;


CUTEST_TEST_SETUP(copy) {
  blosc2_init();

  // Add parametrizations
  CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(
          2,
          4,
  ));
  CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
          {2, {30, 30}, {20, 20}, {10, 10},
           {20, 20}, {10, 10}},
          {3, {40, 15, 23}, {31, 5, 22}, {4, 4, 4},
           {30, 5, 20}, {10, 4, 4}},
          {3, {40, 0, 12}, {31, 0, 12}, {10, 0, 12},
           {20, 0, 12}, {25, 0, 6}},
// The following cases are skipped to reduce the execution time of the test suite
//            {4, {25, 60, 31, 12}, {12, 20, 20, 10}, {5, 5, 5, 10},
//                {25, 20, 20, 10}, {5, 5, 5, 10}},
//            {5, {1, 1, 1024, 1, 1}, {1, 1, 500, 1, 1}, {1, 1, 200, 1, 1},
//                {1, 1, 300, 1, 1}, {1, 1, 50, 1, 1}},
//            {6, {5, 1, 100, 3, 1, 2}, {4, 1, 50, 2, 1, 2}, {2, 1, 20, 2, 1, 2},
//                {4, 1, 50, 2, 1, 1}, {2, 1, 20, 2, 1, 1}},
  ));
  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
          {false, false},
          {true, false},
          {false, true},
          {true, true},
  ));

  CUTEST_PARAMETRIZE(backend2, _test_backend, CUTEST_DATA(
          {false, false},
          {false, false},
          {true, false},
          {false, true},
          {true, true},
  ));
}

CUTEST_TEST_TEST(copy) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, test_shapes_t);
  CUTEST_GET_PARAMETER(backend2, _test_backend);
  CUTEST_GET_PARAMETER(typesize, uint8_t);

  char *urlpath = "test_copy.b2frame";
  char *urlpath2 = "test_copy2.b2frame";
  blosc2_remove_urlpath(urlpath);
  blosc2_remove_urlpath(urlpath2);

  double datatoserialize = 8.34;

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 2;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage.urlpath = urlpath;
  } else {
    b2_storage.urlpath = NULL;
  }
  b2_storage.contiguous = backend.contiguous;

  int8_t nmetalayers = 1;
  blosc2_metalayer metalayers[CATERVA_MAX_METALAYERS];
  metalayers[0].name = "random";
  metalayers[0].content = (uint8_t *) &datatoserialize;
  metalayers[0].content_len = 8;

  caterva_context_t *ctx = caterva_create_ctx(&b2_storage, shapes.ndim, shapes.shape,
                                              shapes.chunkshape, shapes.blockshape, metalayers, nmetalayers);

  /* Create original data */
  size_t buffersize = typesize;
  for (int i = 0; i < ctx->ndim; ++i) {
    buffersize *= (size_t) ctx->shape[i];
  }
  uint8_t *buffer = malloc(buffersize);
  CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, typesize, (buffersize / typesize)));

  /* Create caterva_array_t with original data */
  caterva_array_t *src;
  CATERVA_TEST_ASSERT(caterva_from_buffer(ctx,
                                          &src, buffer, buffersize));

  /* Assert the metalayers creation */
  int rc = blosc2_meta_exists(src->sc, "random");
  if (rc < 0) {
    CATERVA_TEST_ASSERT(CATERVA_ERR_BLOSC_FAILED);
  }
  uint8_t *content;
  int32_t content_len;
  CATERVA_TEST_ASSERT(blosc2_meta_get(src->sc, "random", &content, &content_len));
  double serializeddata = *((double *) content);
  if (serializeddata != datatoserialize) {
    CATERVA_TEST_ASSERT(CATERVA_ERR_BLOSC_FAILED);
  }

  CATERVA_TEST_ASSERT(blosc2_vlmeta_add(src->sc, "random", content, content_len,
                                        src->sc->storage->cparams));
  free(content);


  /* Create storage for dest container */
  if (backend2.persistent) {
    b2_storage.urlpath = urlpath2;
  } else {
    b2_storage.urlpath = NULL;
  }
  b2_storage.contiguous = backend2.contiguous;
  caterva_context_t *ctx2 = caterva_create_ctx(&b2_storage, shapes.ndim, shapes.shape,
                                                  shapes.chunkshape2, shapes.blockshape2, NULL, 0);

  caterva_array_t *dest;
  CATERVA_TEST_ASSERT(caterva_copy(ctx2, src, &dest));

  /* Assert the metalayers creation */
  CATERVA_TEST_ASSERT(blosc2_meta_get(dest->sc, "random", &content, &content_len));
  serializeddata = *((double *) content);
  if (serializeddata != datatoserialize) {
    CATERVA_TEST_ASSERT(CATERVA_ERR_BLOSC_FAILED);
  }
  free(content);

  CATERVA_TEST_ASSERT(blosc2_vlmeta_get(dest->sc, "random", &content, &content_len));
  serializeddata = *((double *) content);
  if (serializeddata != datatoserialize) {
    CATERVA_TEST_ASSERT(CATERVA_ERR_BLOSC_FAILED);
  }
  free(content);


  uint8_t *buffer_dest = malloc(buffersize);
  CATERVA_TEST_ASSERT(caterva_to_buffer(dest, buffer_dest, buffersize));

  /* Testing */
  CATERVA_TEST_ASSERT_BUFFER(buffer, buffer_dest, (int) buffersize);

  /* Free mallocs */
  free(buffer);
  free(buffer_dest);
  CATERVA_TEST_ASSERT(caterva_free(src));
  CATERVA_TEST_ASSERT(caterva_free(dest));
  CATERVA_TEST_ASSERT(caterva_free_ctx(ctx));
  CATERVA_TEST_ASSERT(caterva_free_ctx(ctx2));

  blosc2_remove_urlpath(urlpath);
  blosc2_remove_urlpath(urlpath2);

  return 0;
}

CUTEST_TEST_TEARDOWN(copy) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(copy);
}
