// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_EFFECTS_SKYUV2RGBSHADER_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_EFFECTS_SKYUV2RGBSHADER_H_

#include "SkShader.h"

class SkYUV2RGBShader : public SkShader {
 public:
  SkYUV2RGBShader(SkYUVColorSpace color_space,
                  const SkBitmap& y_bitmap, const SkMatrix& y_matrix,
                  const SkBitmap& u_bitmap, const SkMatrix& u_matrix,
                  const SkBitmap& v_bitmap, const SkMatrix& v_matrix);
  virtual ~SkYUV2RGBShader();

  virtual size_t contextSize() const SK_OVERRIDE;

  class YUV2RGBShaderContext : public SkShader::Context {
   public:
    // Takes ownership of shaderContext and calls its destructor.
    YUV2RGBShaderContext(SkYUVColorSpace color_space,
                         const SkYUV2RGBShader& yuv2rgb_shader,
                         SkShader::Context* y_shader_context,
                         SkShader::Context* u_shader_context,
                         SkShader::Context* v_shader_context,
                         const ContextRec& rec);
    virtual ~YUV2RGBShaderContext();

    virtual uint32_t getFlags() const SK_OVERRIDE;

    virtual void shadeSpan(int x, int y, SkPMColor[], int count) SK_OVERRIDE;
    virtual void shadeSpan16(int x, int y, uint16_t[], int count) SK_OVERRIDE;

    virtual void set3DMask(const SkMask* mask) SK_OVERRIDE {
        // forward to our proxy
        y_shader_context_->set3DMask(mask);
        u_shader_context_->set3DMask(mask);
        v_shader_context_->set3DMask(mask);
    }

   private:
    SkYUVColorSpace color_space_;

    SkShader::Context* y_shader_context_;
    SkShader::Context* u_shader_context_;
    SkShader::Context* v_shader_context_;

    typedef SkShader::Context INHERITED;
  };

#if SK_SUPPORT_GPU
  bool asFragmentProcessor(
      GrContext*, const SkPaint&, const SkMatrix*, GrColor*,
      GrFragmentProcessor**) const SK_OVERRIDE;
#endif

  SK_TO_STRING_OVERRIDE()
  SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(SkYUV2RGBShader)

 protected:
#ifdef SK_SUPPORT_LEGACY_DEEPFLATTENING
  explicit SkYUV2RGBShader(SkReadBuffer&);
#endif
  virtual void flatten(SkWriteBuffer&) const SK_OVERRIDE;
  virtual Context* onCreateContext(
      const ContextRec&, void* storage) const SK_OVERRIDE;

 private:
  void InitializeShaders();

  SkYUVColorSpace color_space_;

  SkBitmap y_bitmap_;
  SkMatrix y_matrix_;
  SkShader* y_shader_;

  SkBitmap u_bitmap_;
  SkMatrix u_matrix_;
  SkShader* u_shader_;

  SkBitmap v_bitmap_;
  SkMatrix v_matrix_;
  SkShader* v_shader_;

  typedef SkShader INHERITED;
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_EFFECTS_SKYUV2RGBSHADER_H_