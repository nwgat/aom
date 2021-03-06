/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
*/


#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/entropy.h"
#include "av1/common/scan.h"
#include "aom/aom_codec.h"
#include "aom/aom_integer.h"

using libaom_test::ACMRandom;

namespace {
#if !CONFIG_AOM_QM
#if CONFIG_AOM_HIGHBITDEPTH
const int number_of_iterations = 100;

typedef void (*QuantizeFunc)(const tran_low_t *coeff, intptr_t count,
                             int skip_block, const int16_t *zbin,
                             const int16_t *round, const int16_t *quant,
                             const int16_t *quant_shift, tran_low_t *qcoeff,
                             tran_low_t *dqcoeff, const int16_t *dequant,
                             uint16_t *eob, const int16_t *scan,
                             const int16_t *iscan);
typedef std::tr1::tuple<QuantizeFunc, QuantizeFunc, aom_bit_depth_t>
    QuantizeParam;

class AV1QuantizeTest : public ::testing::TestWithParam<QuantizeParam> {
 public:
  virtual ~AV1QuantizeTest() {}
  virtual void SetUp() {
    quantize_op_ = GET_PARAM(0);
    ref_quantize_op_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
    mask_ = (1 << bit_depth_) - 1;
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  aom_bit_depth_t bit_depth_;
  int mask_;
  QuantizeFunc quantize_op_;
  QuantizeFunc ref_quantize_op_;
};

class AV1Quantize32Test : public ::testing::TestWithParam<QuantizeParam> {
 public:
  virtual ~AV1Quantize32Test() {}
  virtual void SetUp() {
    quantize_op_ = GET_PARAM(0);
    ref_quantize_op_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
    mask_ = (1 << bit_depth_) - 1;
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  aom_bit_depth_t bit_depth_;
  int mask_;
  QuantizeFunc quantize_op_;
  QuantizeFunc ref_quantize_op_;
};

TEST_P(AV1QuantizeTest, OperationCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, tran_low_t, coeff_ptr[256]);
  DECLARE_ALIGNED(16, int16_t, zbin_ptr[2]);
  DECLARE_ALIGNED(16, int16_t, round_ptr[2]);
  DECLARE_ALIGNED(16, int16_t, quant_ptr[2]);
  DECLARE_ALIGNED(16, int16_t, quant_shift_ptr[2]);
  DECLARE_ALIGNED(16, tran_low_t, qcoeff_ptr[256]);
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff_ptr[256]);
  DECLARE_ALIGNED(16, tran_low_t, ref_qcoeff_ptr[256]);
  DECLARE_ALIGNED(16, tran_low_t, ref_dqcoeff_ptr[256]);
  DECLARE_ALIGNED(16, int16_t, dequant_ptr[2]);
  DECLARE_ALIGNED(16, uint16_t, eob_ptr[1]);
  DECLARE_ALIGNED(16, uint16_t, ref_eob_ptr[1]);
  int err_count_total = 0;
  int first_failure = -1;
  for (int i = 0; i < number_of_iterations; ++i) {
    const int skip_block = i == 0;
    const TX_SIZE sz = (TX_SIZE)(i % 3);  // TX_4X4, TX_8X8 TX_16X16
    const TX_TYPE tx_type = (TX_TYPE)((i >> 2) % 3);
    const scan_order *scan_order = &av1_scan_orders[sz][tx_type];
    const int count = (4 << sz) * (4 << sz);  // 16, 64, 256
    int err_count = 0;
    *eob_ptr = rnd.Rand16();
    *ref_eob_ptr = *eob_ptr;
    for (int j = 0; j < count; j++) {
      coeff_ptr[j] = rnd.Rand16() & mask_;
    }
    for (int j = 0; j < 2; j++) {
      zbin_ptr[j] = rnd.Rand16() & mask_;
      round_ptr[j] = rnd.Rand16();
      quant_ptr[j] = rnd.Rand16();
      quant_shift_ptr[j] = rnd.Rand16();
      dequant_ptr[j] = rnd.Rand16();
    }
    ref_quantize_op_(coeff_ptr, count, skip_block, zbin_ptr, round_ptr,
                     quant_ptr, quant_shift_ptr, ref_qcoeff_ptr,
                     ref_dqcoeff_ptr, dequant_ptr, ref_eob_ptr,
                     scan_order->scan, scan_order->iscan);
    ASM_REGISTER_STATE_CHECK(quantize_op_(
        coeff_ptr, count, skip_block, zbin_ptr, round_ptr, quant_ptr,
        quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr,
        scan_order->scan, scan_order->iscan));
    for (int j = 0; j < sz; ++j) {
      err_count += (ref_qcoeff_ptr[j] != qcoeff_ptr[j]) |
                   (ref_dqcoeff_ptr[j] != dqcoeff_ptr[j]);
    }
    err_count += (*ref_eob_ptr != *eob_ptr);
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Quantization Test, C output doesn't match SSE2 output. "
      << "First failed at test case " << first_failure;
}

TEST_P(AV1Quantize32Test, OperationCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, tran_low_t, coeff_ptr[1024]);
  DECLARE_ALIGNED(16, int16_t, zbin_ptr[2]);
  DECLARE_ALIGNED(16, int16_t, round_ptr[2]);
  DECLARE_ALIGNED(16, int16_t, quant_ptr[2]);
  DECLARE_ALIGNED(16, int16_t, quant_shift_ptr[2]);
  DECLARE_ALIGNED(16, tran_low_t, qcoeff_ptr[1024]);
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff_ptr[1024]);
  DECLARE_ALIGNED(16, tran_low_t, ref_qcoeff_ptr[1024]);
  DECLARE_ALIGNED(16, tran_low_t, ref_dqcoeff_ptr[1024]);
  DECLARE_ALIGNED(16, int16_t, dequant_ptr[2]);
  DECLARE_ALIGNED(16, uint16_t, eob_ptr[1]);
  DECLARE_ALIGNED(16, uint16_t, ref_eob_ptr[1]);
  int err_count_total = 0;
  int first_failure = -1;
  for (int i = 0; i < number_of_iterations; ++i) {
    const int skip_block = i == 0;
    const TX_SIZE sz = TX_32X32;
    const TX_TYPE tx_type = (TX_TYPE)(i % 4);
    const scan_order *scan_order = &av1_scan_orders[sz][tx_type];
    const int count = (4 << sz) * (4 << sz);  // 1024
    int err_count = 0;
    *eob_ptr = rnd.Rand16();
    *ref_eob_ptr = *eob_ptr;
    for (int j = 0; j < count; j++) {
      coeff_ptr[j] = rnd.Rand16() & mask_;
    }
    for (int j = 0; j < 2; j++) {
      zbin_ptr[j] = rnd.Rand16() & mask_;
      round_ptr[j] = rnd.Rand16();
      quant_ptr[j] = rnd.Rand16();
      quant_shift_ptr[j] = rnd.Rand16();
      dequant_ptr[j] = rnd.Rand16();
    }
    ref_quantize_op_(coeff_ptr, count, skip_block, zbin_ptr, round_ptr,
                     quant_ptr, quant_shift_ptr, ref_qcoeff_ptr,
                     ref_dqcoeff_ptr, dequant_ptr, ref_eob_ptr,
                     scan_order->scan, scan_order->iscan);
    ASM_REGISTER_STATE_CHECK(quantize_op_(
        coeff_ptr, count, skip_block, zbin_ptr, round_ptr, quant_ptr,
        quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr,
        scan_order->scan, scan_order->iscan));
    for (int j = 0; j < sz; ++j) {
      err_count += (ref_qcoeff_ptr[j] != qcoeff_ptr[j]) |
                   (ref_dqcoeff_ptr[j] != dqcoeff_ptr[j]);
    }
    err_count += (*ref_eob_ptr != *eob_ptr);
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Quantization Test, C output doesn't match SSE2 output. "
      << "First failed at test case " << first_failure;
}

TEST_P(AV1QuantizeTest, EOBCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, tran_low_t, coeff_ptr[256]);
  DECLARE_ALIGNED(16, int16_t, zbin_ptr[2]);
  DECLARE_ALIGNED(16, int16_t, round_ptr[2]);
  DECLARE_ALIGNED(16, int16_t, quant_ptr[2]);
  DECLARE_ALIGNED(16, int16_t, quant_shift_ptr[2]);
  DECLARE_ALIGNED(16, tran_low_t, qcoeff_ptr[256]);
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff_ptr[256]);
  DECLARE_ALIGNED(16, tran_low_t, ref_qcoeff_ptr[256]);
  DECLARE_ALIGNED(16, tran_low_t, ref_dqcoeff_ptr[256]);
  DECLARE_ALIGNED(16, int16_t, dequant_ptr[2]);
  DECLARE_ALIGNED(16, uint16_t, eob_ptr[1]);
  DECLARE_ALIGNED(16, uint16_t, ref_eob_ptr[1]);
  int err_count_total = 0;
  int first_failure = -1;
  for (int i = 0; i < number_of_iterations; ++i) {
    int skip_block = i == 0;
    TX_SIZE sz = (TX_SIZE)(i % 3);  // TX_4X4, TX_8X8 TX_16X16
    TX_TYPE tx_type = (TX_TYPE)((i >> 2) % 3);
    const scan_order *scan_order = &av1_scan_orders[sz][tx_type];
    int count = (4 << sz) * (4 << sz);  // 16, 64, 256
    int err_count = 0;
    *eob_ptr = rnd.Rand16();
    *ref_eob_ptr = *eob_ptr;
    // Two random entries
    for (int j = 0; j < count; j++) {
      coeff_ptr[j] = 0;
    }
    coeff_ptr[rnd(count)] = rnd.Rand16() & mask_;
    coeff_ptr[rnd(count)] = rnd.Rand16() & mask_;
    for (int j = 0; j < 2; j++) {
      zbin_ptr[j] = rnd.Rand16() & mask_;
      round_ptr[j] = rnd.Rand16();
      quant_ptr[j] = rnd.Rand16();
      quant_shift_ptr[j] = rnd.Rand16();
      dequant_ptr[j] = rnd.Rand16();
    }

    ref_quantize_op_(coeff_ptr, count, skip_block, zbin_ptr, round_ptr,
                     quant_ptr, quant_shift_ptr, ref_qcoeff_ptr,
                     ref_dqcoeff_ptr, dequant_ptr, ref_eob_ptr,
                     scan_order->scan, scan_order->iscan);
    ASM_REGISTER_STATE_CHECK(quantize_op_(
        coeff_ptr, count, skip_block, zbin_ptr, round_ptr, quant_ptr,
        quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr,
        scan_order->scan, scan_order->iscan));

    for (int j = 0; j < sz; ++j) {
      err_count += (ref_qcoeff_ptr[j] != qcoeff_ptr[j]) |
                   (ref_dqcoeff_ptr[j] != dqcoeff_ptr[j]);
    }
    err_count += (*ref_eob_ptr != *eob_ptr);
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Quantization Test, C output doesn't match SSE2 output. "
      << "First failed at test case " << first_failure;
}

TEST_P(AV1Quantize32Test, EOBCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, tran_low_t, coeff_ptr[1024]);
  DECLARE_ALIGNED(16, int16_t, zbin_ptr[2]);
  DECLARE_ALIGNED(16, int16_t, round_ptr[2]);
  DECLARE_ALIGNED(16, int16_t, quant_ptr[2]);
  DECLARE_ALIGNED(16, int16_t, quant_shift_ptr[2]);
  DECLARE_ALIGNED(16, tran_low_t, qcoeff_ptr[1024]);
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff_ptr[1024]);
  DECLARE_ALIGNED(16, tran_low_t, ref_qcoeff_ptr[1024]);
  DECLARE_ALIGNED(16, tran_low_t, ref_dqcoeff_ptr[1024]);
  DECLARE_ALIGNED(16, int16_t, dequant_ptr[2]);
  DECLARE_ALIGNED(16, uint16_t, eob_ptr[1]);
  DECLARE_ALIGNED(16, uint16_t, ref_eob_ptr[1]);
  int err_count_total = 0;
  int first_failure = -1;
  for (int i = 0; i < number_of_iterations; ++i) {
    int skip_block = i == 0;
    TX_SIZE sz = TX_32X32;
    TX_TYPE tx_type = (TX_TYPE)(i % 4);
    const scan_order *scan_order = &av1_scan_orders[sz][tx_type];
    int count = (4 << sz) * (4 << sz);  // 1024
    int err_count = 0;
    *eob_ptr = rnd.Rand16();
    *ref_eob_ptr = *eob_ptr;
    for (int j = 0; j < count; j++) {
      coeff_ptr[j] = 0;
    }
    // Two random entries
    coeff_ptr[rnd(count)] = rnd.Rand16() & mask_;
    coeff_ptr[rnd(count)] = rnd.Rand16() & mask_;
    for (int j = 0; j < 2; j++) {
      zbin_ptr[j] = rnd.Rand16() & mask_;
      round_ptr[j] = rnd.Rand16();
      quant_ptr[j] = rnd.Rand16();
      quant_shift_ptr[j] = rnd.Rand16();
      dequant_ptr[j] = rnd.Rand16();
    }

    ref_quantize_op_(coeff_ptr, count, skip_block, zbin_ptr, round_ptr,
                     quant_ptr, quant_shift_ptr, ref_qcoeff_ptr,
                     ref_dqcoeff_ptr, dequant_ptr, ref_eob_ptr,
                     scan_order->scan, scan_order->iscan);
    ASM_REGISTER_STATE_CHECK(quantize_op_(
        coeff_ptr, count, skip_block, zbin_ptr, round_ptr, quant_ptr,
        quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr,
        scan_order->scan, scan_order->iscan));

    for (int j = 0; j < sz; ++j) {
      err_count += (ref_qcoeff_ptr[j] != qcoeff_ptr[j]) |
                   (ref_dqcoeff_ptr[j] != dqcoeff_ptr[j]);
    }
    err_count += (*ref_eob_ptr != *eob_ptr);
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Quantization Test, C output doesn't match SSE2 output. "
      << "First failed at test case " << first_failure;
}
using std::tr1::make_tuple;

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, AV1QuantizeTest,
    ::testing::Values(make_tuple(&aom_highbd_quantize_b_sse2,
                                 &aom_highbd_quantize_b_c, AOM_BITS_8),
                      make_tuple(&aom_highbd_quantize_b_sse2,
                                 &aom_highbd_quantize_b_c, AOM_BITS_10),
                      make_tuple(&aom_highbd_quantize_b_sse2,
                                 &aom_highbd_quantize_b_c, AOM_BITS_12)));
INSTANTIATE_TEST_CASE_P(
    SSE2, AV1Quantize32Test,
    ::testing::Values(make_tuple(&aom_highbd_quantize_b_32x32_sse2,
                                 &aom_highbd_quantize_b_32x32_c, AOM_BITS_8),
                      make_tuple(&aom_highbd_quantize_b_32x32_sse2,
                                 &aom_highbd_quantize_b_32x32_c, AOM_BITS_10),
                      make_tuple(&aom_highbd_quantize_b_32x32_sse2,
                                 &aom_highbd_quantize_b_32x32_c, AOM_BITS_12)));
#endif  // HAVE_SSE2
#endif  // CONFIG_AOM_HIGHBITDEPTH
#endif  // CONFIG_AOM_QM
}  // namespace
