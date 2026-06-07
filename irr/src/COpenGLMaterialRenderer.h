// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#ifdef _IRR_COMPILE_WITH_OPENGL_

#include "IMaterialRenderer.h"

#include "COpenGLDriver.h"
#include "COpenGLCacheHandler.h"

namespace video
{

//! Solid material renderer
class COpenGLMaterialRenderer_SOLID : public IMaterialRenderer
{
public:
	COpenGLMaterialRenderer_SOLID(video::COpenGLDriver *d) :
			Driver(d) {}

	virtual void OnSetMaterial(const SMaterial &material, const SMaterial &lastMaterial,
			bool resetAllRenderstates, IMaterialRendererServices *services) override
	{
		if (Driver->getFixedPipelineState() == COpenGLDriver::EOFPS_DISABLE)
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_DISABLE_TO_ENABLE);
		else
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_ENABLE);

		Driver->disableTextures(1);
		Driver->setBasicRenderStates(material, lastMaterial, resetAllRenderstates);

		if (resetAllRenderstates || material.MaterialType != lastMaterial.MaterialType) {
			Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
	}

protected:
	video::COpenGLDriver *Driver;
};

//! Generic Texture Blend
class COpenGLMaterialRenderer_ONETEXTURE_BLEND : public IMaterialRenderer
{
public:
	COpenGLMaterialRenderer_ONETEXTURE_BLEND(video::COpenGLDriver *d) :
			Driver(d) {}

	virtual void OnSetMaterial(const SMaterial &material, const SMaterial &lastMaterial,
			bool resetAllRenderstates, IMaterialRendererServices *services) override
	{
		if (Driver->getFixedPipelineState() == COpenGLDriver::EOFPS_DISABLE)
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_DISABLE_TO_ENABLE);
		else
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_ENABLE);

		Driver->disableTextures(1);
		Driver->setBasicRenderStates(material, lastMaterial, resetAllRenderstates);

		//		if (material.MaterialType != lastMaterial.MaterialType ||
		//			material.MaterialTypeParam != lastMaterial.MaterialTypeParam ||
		//			resetAllRenderstates)
		{
			E_BLEND_FACTOR srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact;
			E_MODULATE_FUNC modulate;
			u32 alphaSource;
			unpack_textureBlendFuncSeparate(srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact, modulate, alphaSource, material.MaterialTypeParam);

			Driver->getCacheHandler()->setBlend(true);

			if (Driver->queryFeature(EVDF_BLEND_SEPARATE)) {
				Driver->getCacheHandler()->setBlendFuncSeparate(Driver->getGLBlend(srcRGBFact), Driver->getGLBlend(dstRGBFact),
						Driver->getGLBlend(srcAlphaFact), Driver->getGLBlend(dstAlphaFact));
			} else {
				Driver->getCacheHandler()->setBlendFunc(Driver->getGLBlend(srcRGBFact), Driver->getGLBlend(dstRGBFact));
			}

			Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);

			if (Driver->queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_env_combine)) {
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
				glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, (f32)modulate);
			} else {
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}

			if (textureBlendFunc_hasAlpha(srcRGBFact) || textureBlendFunc_hasAlpha(dstRGBFact) ||
					textureBlendFunc_hasAlpha(srcAlphaFact) || textureBlendFunc_hasAlpha(dstAlphaFact)) {
				if (alphaSource == EAS_VERTEX_COLOR) {
					if (Driver->queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_env_combine)) {
						glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
						glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
					} else {
						glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
					}
				} else if (alphaSource == EAS_TEXTURE) {
					if (Driver->queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_env_combine)) {
						glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
						glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
					} else {
						glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
					}
				} else {
					if (Driver->queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_env_combine)) {
						glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
						glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
						glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE);
					} else {
						glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
					}
				}
			}
		}
	}

	void OnUnsetMaterial() override
	{
		Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		if (Driver->queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_env_combine)) {
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 1.f);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
		}

		Driver->getCacheHandler()->setBlend(false);
	}

	//! Returns if the material is transparent.
	/** Is not always transparent, but mostly. */
	bool isTransparent() const override
	{
		return true;
	}

protected:
	video::COpenGLDriver *Driver;
};

//! Transparent vertex alpha material renderer
class COpenGLMaterialRenderer_TRANSPARENT_VERTEX_ALPHA : public IMaterialRenderer
{
public:
	COpenGLMaterialRenderer_TRANSPARENT_VERTEX_ALPHA(video::COpenGLDriver *d) :
			Driver(d) {}

	virtual void OnSetMaterial(const SMaterial &material, const SMaterial &lastMaterial,
			bool resetAllRenderstates, IMaterialRendererServices *services) override
	{
		if (Driver->getFixedPipelineState() == COpenGLDriver::EOFPS_DISABLE)
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_DISABLE_TO_ENABLE);
		else
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_ENABLE);

		Driver->disableTextures(1);
		Driver->setBasicRenderStates(material, lastMaterial, resetAllRenderstates);

		Driver->getCacheHandler()->setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		Driver->getCacheHandler()->setBlend(true);

		if (material.MaterialType != lastMaterial.MaterialType || resetAllRenderstates) {
			Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);

			if (Driver->queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_env_combine)) {
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
			} else {
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}
		}
	}

	void OnUnsetMaterial() override
	{
		Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		if (Driver->queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_env_combine)) {
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
		}

		Driver->getCacheHandler()->setBlend(false);
	}

	//! Returns if the material is transparent.
	bool isTransparent() const override
	{
		return true;
	}

protected:
	video::COpenGLDriver *Driver;
};

//! Transparent alpha channel material renderer
class COpenGLMaterialRenderer_TRANSPARENT_ALPHA_CHANNEL : public IMaterialRenderer
{
public:
	COpenGLMaterialRenderer_TRANSPARENT_ALPHA_CHANNEL(video::COpenGLDriver *d) :
			Driver(d) {}

	virtual void OnSetMaterial(const SMaterial &material, const SMaterial &lastMaterial,
			bool resetAllRenderstates, IMaterialRendererServices *services) override
	{
		if (Driver->getFixedPipelineState() == COpenGLDriver::EOFPS_DISABLE)
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_DISABLE_TO_ENABLE);
		else
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_ENABLE);

		Driver->disableTextures(1);
		Driver->setBasicRenderStates(material, lastMaterial, resetAllRenderstates);

		Driver->getCacheHandler()->setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		Driver->getCacheHandler()->setBlend(true);
		Driver->getCacheHandler()->setAlphaTest(true);
		Driver->getCacheHandler()->setAlphaFunc(GL_GREATER, material.MaterialTypeParam);

		if (material.MaterialType != lastMaterial.MaterialType || resetAllRenderstates || material.MaterialTypeParam != lastMaterial.MaterialTypeParam) {
			Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);

			if (Driver->queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_env_combine)) {
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
			} else {
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}
		}
	}

	void OnUnsetMaterial() override
	{
		Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		if (Driver->queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_env_combine)) {
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
		}
		Driver->getCacheHandler()->setAlphaTest(false);
		Driver->getCacheHandler()->setBlend(false);
	}

	//! Returns if the material is transparent.
	bool isTransparent() const override
	{
		return true;
	}

protected:
	video::COpenGLDriver *Driver;
};

//! Transparent alpha channel material renderer
class COpenGLMaterialRenderer_TRANSPARENT_ALPHA_CHANNEL_REF : public IMaterialRenderer
{
public:
	COpenGLMaterialRenderer_TRANSPARENT_ALPHA_CHANNEL_REF(video::COpenGLDriver *d) :
			Driver(d) {}

	virtual void OnSetMaterial(const SMaterial &material, const SMaterial &lastMaterial,
			bool resetAllRenderstates, IMaterialRendererServices *services) override
	{
		if (Driver->getFixedPipelineState() == COpenGLDriver::EOFPS_DISABLE)
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_DISABLE_TO_ENABLE);
		else
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_ENABLE);

		Driver->disableTextures(1);
		Driver->setBasicRenderStates(material, lastMaterial, resetAllRenderstates);

		if (material.MaterialType != lastMaterial.MaterialType || resetAllRenderstates) {
			Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

			Driver->getCacheHandler()->setAlphaTest(true);
			Driver->getCacheHandler()->setAlphaFunc(GL_GREATER, 0.5f);
		}
	}

	void OnUnsetMaterial() override
	{
		Driver->getCacheHandler()->setAlphaTest(false);
	}

	//! Returns if the material is transparent.
	bool isTransparent() const override
	{
		return false; // this material is not really transparent because it does no blending.
	}

protected:
	video::COpenGLDriver *Driver;
};

} // end namespace video

#endif
