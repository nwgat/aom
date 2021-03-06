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

#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

class AqSegmentTest
    : public ::libaom_test::EncoderTest,
      public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int> {
 protected:
  AqSegmentTest() : EncoderTest(GET_PARAM(0)) {}
  virtual ~AqSegmentTest() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    set_cpu_used_ = GET_PARAM(2);
    aq_mode_ = 0;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 1) {
      encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
      encoder->Control(AV1E_SET_AQ_MODE, aq_mode_);
      encoder->Control(AOME_SET_MAX_INTRA_BITRATE_PCT, 100);
    }
  }

  int set_cpu_used_;
  int aq_mode_;
};

// Validate that this AQ segmentation mode (AQ=1, variance_ap)
// encodes and decodes without a mismatch.
TEST_P(AqSegmentTest, TestNoMisMatchAQ1) {
  cfg_.rc_min_quantizer = 8;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = AOM_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_target_bitrate = 300;

  aq_mode_ = 1;

  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 100);

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

// Validate that this AQ segmentation mode (AQ=2, complexity_aq)
// encodes and decodes without a mismatch.
TEST_P(AqSegmentTest, TestNoMisMatchAQ2) {
  cfg_.rc_min_quantizer = 8;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = AOM_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_target_bitrate = 300;

  aq_mode_ = 2;

  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 100);

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

// Validate that this AQ segmentation mode (AQ=3, cyclic_refresh_aq)
// encodes and decodes without a mismatch.
TEST_P(AqSegmentTest, TestNoMisMatchAQ3) {
  cfg_.rc_min_quantizer = 8;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = AOM_CBR;
  cfg_.g_lag_in_frames = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_target_bitrate = 300;

  aq_mode_ = 3;

  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 100);

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

AV1_INSTANTIATE_TEST_CASE(AqSegmentTest,
                           ::testing::Values(::libaom_test::kRealTime,
                                             ::libaom_test::kOnePassGood),
                           ::testing::Range(3, 9));
}  // namespace
