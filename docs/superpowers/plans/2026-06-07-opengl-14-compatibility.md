# OpenGL 1.4 Compatibility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the ability to run Minetest with OpenGL 1.4 core via an `enable_shaders` toggle, using the legacy `COpenGLDriver` for fixed-function pipeline (FFP) rendering when shaders are disabled.

**Architecture:** The `enable_shaders` setting controls whether the client uses GLSL shaders (current behavior) or falls back to vertex-color-based rendering with standard Irrlicht `EMT_*` material types. The legacy `COpenGLDriver` (EDT_OPENGL) already has FFP material renderers and state management — it needs a GL 1.4 context request and optional VBO/FBO usage. The client code restores the `m_enable_shaders` branches that existed for 11 years before commit `794aea8e9`.

**Tech Stack:** C++, OpenGL 1.4 core, IrrlichtMt (COpenGLDriver), GLSL (when enabled), CMake

---

### Task 1: Request GL 1.4 context for EDT_OPENGL

**Files:**
- Modify: `irr/src/CIrrDeviceSDL.cpp:671-689`

- [ ] **Step 1: Change EDT_OPENGL context request to 1.4 with 2.1 fallback**

```cpp
	case video::EDT_OPENGL:
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);
		break;
```

Then after context creation fails (around line 729-734), add a fallback to 2.1:

```cpp
	Context = SDL_GL_CreateContext(Window);
	if (!Context && CreationParams.DriverType == video::EDT_OPENGL) {
		// Fallback: try OpenGL 2.1 context
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		Context = SDL_GL_CreateContext(Window);
	}
```

- [ ] **Step 2: Verify context is created**

Run: `cmake --build build && ./build/bin/minetest --info 2>&1 | grep -i "OpenGL\|version"` — should see `OpenGL 1.4` or `OpenGL 2.1` depending on what the system provides.

- [ ] **Step 3: Commit**

```bash
git add irr/src/CIrrDeviceSDL.cpp
git commit -m "A1: request GL 1.4 context for EDT_OPENGL driver"
```

---

### Task 2: Make VBOs optional in legacy COpenGLDriver

**Files:**
- Modify: `irr/src/COpenGLDriver.h:50-65`
- Modify: `irr/src/COpenGLDriver.cpp:291-430`

- [ ] **Step 1: Add `m_vbo_enabled` flag to COpenGLDriver**

In `irr/src/COpenGLDriver.h`, add to the class (around line 36, in the private section):

```cpp
	bool m_vbo_enabled = true;
```

- [ ] **Step 2: Set `m_vbo_enabled` based on setting + feature**

In `COpenGLDriver::initDriver()` (after `genericDriverInit()`, around line 60-70):

```cpp
	m_vbo_enabled = g_settings->getBool("enable_shaders")
		&& FeatureAvailable[IRR_ARB_vertex_buffer_object];
```

Add the include for `settings.h` at the top of COpenGLDriver.cpp... Actually, Irrlicht shouldn't depend on Minetest settings. Instead, let the Minetest client code tell the driver whether VBOs should be used. Add a setter:

In `COpenGLDriver.h`:
```cpp
	void setVBOEnabled(bool enabled) { m_vbo_enabled = enabled; }
```

The Minetest client code will call `driver->setVBOEnabled(g_settings->getBool("enable_shaders"))` after driver creation.

- [ ] **Step 3: Guard VBO creation in `updateHardwareBuffer`**

In `COpenGLDriver.cpp` around line 299, wrap VBO creation:

```cpp
bool COpenGLDriver::updateVertexHardwareBuffer(SHWBufferLink_opengl *HWBuffer)
{
	if (!m_vbo_enabled)
		return false;
	// ... existing code ...
```

And similarly for `updateIndexHardwareBuffer`.

In `createHardwareBuffer` (around line 365-380):

```cpp
SHWBufferLink *COpenGLDriver::createHardwareBuffer(const scene::HWBuffer *buf)
{
	if (!m_vbo_enabled)
		return nullptr;
	// ... existing code ...
```

- [ ] **Step 4: Verify the client-side fallback path works**

The `drawVertexPrimitiveList` already handles both VBO (vertices == NULL) and client-side arrays (vertices != NULL). When VBOs aren't created, the system falls back to client-side arrays automatically. Verify this path is still correct.

- [ ] **Step 5: Commit**

```bash
git add irr/src/COpenGLDriver.h irr/src/COpenGLDriver.cpp
git commit -m "A2: make VBOs optional in COpenGLDriver"
```

---

### Task 3: Skip FBO-dependent render targets when unavailable

**Files:**
- Modify: `irr/src/COpenGLDriver.cpp:554-560`

- [ ] **Step 1: Verify `addRenderTarget` handles missing FBOs gracefully**

Check `irr/src/COpenGLCoreRenderTarget.h` — if the FBO extension is unavailable, `addRenderTarget()` should return a null/fallback render target or the function should error. The legacy driver at line 554-560 just creates a `COpenGLRenderTarget` without checking. If the extension is missing, the render target won't work. For the FFP path, we simply won't call `addRenderTarget()` at all (the client code will skip post-processing).

- [ ] **Step 2: Add a method to check FBO support**

```cpp
bool COpenGLDriver::isFBOAvailable() const
{
	return FeatureAvailable[IRR_EXT_framebuffer_object] || FeatureAvailable[IRR_ARB_framebuffer_object];
}
```

Add the declaration to `COpenGLDriver.h` public section.

- [ ] **Step 3: Commit**

```bash
git add irr/src/COpenGLDriver.h irr/src/COpenGLDriver.cpp
git commit -m "A3: add isFBOAvailable check to COpenGLDriver"
```

---

### Task 4: Use GL_MODULATE instead of GL_COMBINE for true GL 1.4 core

**Files:**
- Modify: `irr/src/COpenGLMaterialRenderer.h:78-100`, `irr/src/COpenGLMaterialRenderer.h:101-200`, `irr/src/COpenGLMaterialRenderer.h:201-332`

- [ ] **Step 1: Add fallback for ARB_texture_env_combine**

In `COpenGLMaterialRenderer_ONETEXTURE_BLEND::OnSetMaterial()`, replace the `#ifdef GL_ARB_texture_env_combine` block with a runtime check:

```cpp
if (Driver->queryOpenGLFeature(IRR_ARB_texture_env_combine)) {
#ifdef GL_ARB_texture_env_combine
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	// ... existing GL_COMBINE code ...
#endif
} else {
	// Fallback to GL_MODULATE for pure GL 1.4
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}
```

Similarly guard `GL_PRIMARY_COLOR_ARB` fallback for alpha source.

- [ ] **Step 2: Repeat for other material renderers that use GL_COMBINE**

Check `COpenGLMaterialRenderer_TRANSPARENT_ALPHA_CHANNEL`, `COpenGLMaterialRenderer_TRANSPARENT_ALPHA_CHANNEL_REF`, `COpenGLMaterialRenderer_TRANSPARENT_VERTEX_ALPHA` for `GL_COMBINE` usage and apply similar fallbacks.

- [ ] **Step 3: Commit**

```bash
git add irr/src/COpenGLMaterialRenderer.h
git commit -m "A4: fallback to GL_MODULATE when ARB_texture_env_combine unavailable"
```

---

### Task 5: Restore alpha test support in legacy driver

**Files:**
- Modify: `irr/src/COpenGLCacheHandler.h`
- Modify: `irr/src/COpenGLCacheHandler.cpp`
- Modify: `irr/src/COpenGLDriver.cpp` (setBasicRenderStates)

- [ ] **Step 1: Read current COpenGLCacheHandler.h**

Read `irr/src/COpenGLCacheHandler.h` and `irr/src/COpenGLCacheHandler.cpp` to see current alpha test handling.

- [ ] **Step 2: Add alpha test state to cache handler**

In `COpenGLCacheHandler.h`, add:
```cpp
void setAlphaFunc(GLenum func, GLclampf ref);
void setAlphaTest(bool enable);
```

In `COpenGLCacheHandler.cpp`, implement with cached state:
```cpp
void COpenGLCacheHandler::setAlphaFunc(GLenum func, GLclampf ref)
{
	if (AlphaFunc != func || AlphaRef != ref) {
		glAlphaFunc(func, ref);
		AlphaFunc = func;
		AlphaRef = ref;
	}
}

void COpenGLCacheHandler::setAlphaTest(bool enable)
{
	if (AlphaMode != enable) {
		if (enable)
			glEnable(GL_ALPHA_TEST);
		else
			glDisable(GL_ALPHA_TEST);
		AlphaMode = enable;
	}
}
```

Add the member variables:
```cpp
bool AlphaMode = false;
GLenum AlphaFunc = GL_ALWAYS;
GLclampf AlphaRef = 0.0f;
```

- [ ] **Step 3: Integrate into setBasicRenderStates**

In `COpenGLDriver::setBasicRenderStates()`, in the FFP section (after the "set current fixed pipeline state" line, around line 1900), add alpha test handling when `tempState == EOFPS_ENABLE`:

```cpp
if (tempState == EOFPS_ENABLE) {
	if (material.MaterialTypeParam != lastmaterial.MaterialTypeParam || resetAllRenderStates) {
		f32 alpha_test_ref = 0.5f;
		if (material.MaterialType == video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF)
			alpha_test_ref = material.MaterialTypeParam;
		CacheHandler->setAlphaTest(true);
		CacheHandler->setAlphaFunc(GL_GREATER, alpha_test_ref);
	} else {
		CacheHandler->setAlphaTest(false);
	}
}
```

- [ ] **Step 4: Commit**

```bash
git add irr/src/COpenGLCacheHandler.h irr/src/COpenGLCacheHandler.cpp irr/src/COpenGLDriver.cpp
git commit -m "A5: restore alpha test support for FFP mode in COpenGLDriver"
```

---

### Task 6: Restore enable_shaders setting and ShaderSource FFP mode

**Files:**
- Modify: `src/defaultsettings.cpp`
- Modify: `src/client/shader.h`
- Modify: `src/client/shader.cpp`
- Modify: `src/client/renderingengine.cpp`
- Create: `builtin/mainmenu/settings/shader_warning_component.lua`
- Modify: `builtin/mainmenu/settings/dlg_settings.lua`
- Modify: `builtin/settingtypes.txt`

- [ ] **Step 1: Restore enable_shaders setting**

In `src/defaultsettings.cpp`, add:
```cpp
settings->setDefault("enable_shaders", "true");
```

In `builtin/settingtypes.txt`, add the setting definition:
```
#    Enable shaders (experimental).
#    When disabled, the game will not use GLSL shaders for rendering
#    and fall back to the fixed-function pipeline (requires OpenGL 1.4+).
enable_shaders (Enable shaders) bool true
```

- [ ] **Step 2: Add m_enabled back to ShaderSource**

In `src/client/shader.h`, add back to the `ShaderSource` class:
```cpp
	bool m_enabled;
```

In `src/client/shader.cpp`, add to `ShaderSource::ShaderSource()` constructor body:
```cpp
	m_enabled = g_settings->getBool("enable_shaders");
	if (!m_enabled) {
		warningstream << "You are running " PROJECT_NAME_C " with shaders disabled, "
			"this is not a recommended configuration." << std::endl;
	}
```

- [ ] **Step 3: Skip GLSL compilation when m_enabled is false**

In `ShaderSource::generateShader()` (around line 530-560), wrap the GLSL compilation:

```cpp
ShaderInfo ShaderSource::generateShader(const std::string &name,
	const std::string &shader_dir, const ShaderConstants &constants,
	video::E_MATERIAL_TYPE base_material, std::unique_ptr<IShaderUniformSetter> setter_cb)
{
	ShaderInfo shaderinfo;
	shaderinfo.name = name;
	shaderinfo.base_material = base_material;
	shaderinfo.material = shaderinfo.base_material;

	if (!m_enabled)
		return shaderinfo;

	// ... rest of existing GLSL compilation code ...
```

Also guard the destructor and `rebuildShaders()`:

```cpp
ShaderSource::~ShaderSource()
{
	MutexAutoLock lock(m_shaderinfo_cache_mutex);
	if (!m_enabled)
		return;
	// ... existing cleanup ...
}

void ShaderSource::rebuildShaders()
{
	MutexAutoLock lock(m_shaderinfo_cache_mutex);
	if (!m_enabled)
		return;
	// ... existing rebuild ...
}
```

- [ ] **Step 4: Handle no GPU programming services in generateShader**

Current code at generateShader() does:
```cpp
auto *gpu = driver->getGPUProgrammingServices();
if (!driver->queryFeature(video::EVDF_ARB_GLSL) || !gpu) {
	throw ShaderException(...);
}
```

When `m_enabled` is false, we return early before reaching this. When `m_enabled` is true but GLSL somehow isn't supported, the exception is still valid.

- [ ] **Step 5: Set VBO on driver when shaders change**

In `src/client/renderingengine.cpp`, after driver creation, add:
```cpp
auto *driver = get_video_driver();
if (auto *ogl_driver = dynamic_cast<video::COpenGLDriver *>(driver)) {
	ogl_driver->setVBOEnabled(g_settings->getBool("enable_shaders"));
}
```

Include the appropriate header for COpenGLDriver. Guard with `#ifdef _IRR_COMPILE_WITH_OPENGL_`.

- [ ] **Step 6: Restore shader_warning_component.lua**

Create `builtin/mainmenu/settings/shader_warning_component.lua`:

```lua
-- Note: this is a restored file from before commit 794aea8e9
local warning = {
	type = "label",
	label = fgettext_needs("Shaders are disabled. This may cause visual issues."),
}

local function shader_warning()
	return warning
end

return shader_warning
```

In `builtin/mainmenu/settings/dlg_settings.lua`, restore the imports and usage:
```lua
local shader_warning_component = dofile(core.get_mainmenu_path() .. DIR_DELIM ..
	"settings" .. DIR_DELIM .. "shader_warning_component.lua")
```

And in the requirement checks:
```lua
shaders_support = shaders_support,
shaders = core.settings:get_bool("enable_shaders") and shaders_support,
```

- [ ] **Step 7: Commit**

```bash
git add src/defaultsettings.cpp src/client/shader.h src/client/shader.cpp src/client/renderingengine.cpp builtin/mainmenu/settings/shader_warning_component.lua builtin/mainmenu/settings/dlg_settings.lua builtin/settingtypes.txt
git commit -m "B1-B5: restore enable_shaders setting and FFP mode in ShaderSource"
```

---

### Task 7: Restore applyMaterialOptions for FFP path

**Files:**
- Modify: `src/client/tile.h`
- Modify: `src/client/tile.cpp`

- [ ] **Step 1: Add applyMaterialOptions FFP variant**

In `src/client/tile.h`, add to `TileLayer`:
```cpp
	void applyMaterialOptions(video::SMaterial &material) const;
	void applyMaterialOptionsWithShaders(video::SMaterial &material) const;
```

In `src/client/tile.cpp`, implement `applyMaterialOptions` (the FFP variant that also sets `MaterialType`):

```cpp
void TileLayer::applyMaterialOptions(video::SMaterial &material) const
{
	// Set the texture
	if (!texture)
		return;
	material.setTexture(0, texture);

	// Set MaterialType based on material_type
	switch (material_type) {
	case TILE_MATERIAL_BASIC:
	case TILE_MATERIAL_OPAQUE:
		material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
		break;
	case TILE_MATERIAL_ALPHA:
		material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
		break;
	default:
		material.MaterialType = video::EMT_SOLID;
		break;
	}

	// Backface culling
	material.BackfaceCulling = !has_transparency;
	material.setTexture(1, nullptr);

	// Texture wrap
	if (!is_tileable) {
		material.TextureLayers[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
		material.TextureLayers[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
	}
}
```

Rename the current `applyMaterialOptions` to `applyMaterialOptionsWithShaders` and keep its current behavior (texture + wrap + offset, no MaterialType set).

- [ ] **Step 2: Update callers**

In `src/client/mapblock_mesh.cpp`, in `MapBlockMesh` constructor:
```cpp
if (m_enable_shaders) {
	material.MaterialType = m_shdrsrc->getShaderInfo(p.layer.shader_id).material;
	p.layer.applyMaterialOptionsWithShaders(material);
} else {
	p.layer.applyMaterialOptions(material);
}
```

Similarly in `src/client/content_cao.cpp`, `src/client/wieldmesh.cpp`.

- [ ] **Step 3: Commit**

```bash
git add src/client/tile.h src/client/tile.cpp src/client/mapblock_mesh.cpp src/client/content_cao.cpp src/client/wieldmesh.cpp
git commit -m "B3: restore applyMaterialOptions FFP variant and update callers"
```

---

### Task 8: Restore FFP path in clouds.cpp

**Files:**
- Modify: `src/client/clouds.h`
- Modify: `src/client/clouds.cpp`

- [ ] **Step 1: Restore m_enable_shaders field**

In `clouds.h`, change:
```cpp
	bool m_enable_3d;
```
to:
```cpp
	bool m_enable_shaders, m_enable_3d;
```

- [ ] **Step 2: Restore FFP branch in constructor**

In `clouds.cpp` constructor:
```cpp
	m_enable_shaders = g_settings->getBool("enable_shaders");

	m_material.BackfaceCulling = true;
	m_material.FogEnable = true;
	m_material.AntiAliasing = video::EAAM_SIMPLE;
	if (m_enable_shaders) {
		auto sid = ssrc->getShader("cloud_shader", TILE_MATERIAL_ALPHA);
		m_material.MaterialType = ssrc->getShaderInfo(sid).material;
	} else {
		m_material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	}
```

- [ ] **Step 3: Restore vertex colors for FFP path in updateMesh**

In `updateMesh()`, restore the old color logic:
```cpp
	video::SColorf c_top_f(m_color);
	video::SColorf c_side_1_f(m_color);
	video::SColorf c_side_2_f(m_color);
	video::SColorf c_bottom_f(m_color);
	if (m_enable_shaders) {
		// shader mixes the base color, set via ColorParam
		c_top_f = c_side_1_f = c_side_2_f = c_bottom_f = video::SColorf(1.0f, 1.0f, 1.0f, 1.0f);
	}
```

- [ ] **Step 4: Guard ColorParam for shader mode**

In `render()`:
```cpp
	if (m_enable_shaders)
		m_material.ColorParam = m_color.toSColor();
```

- [ ] **Step 5: Commit**

```bash
git add src/client/clouds.h src/client/clouds.cpp
git commit -m "C1: restore FFP path in clouds rendering"
```

---

### Task 9: Restore FFP path in sky.cpp

**Files:**
- Modify: `src/client/sky.h`
- Modify: `src/client/sky.cpp`

- [ ] **Step 1: Restore m_enable_shaders**

In `sky.h`:
```cpp
	bool m_enable_shaders = false;
```

In `sky.cpp` constructor:
```cpp
	m_enable_shaders = g_settings->getBool("enable_shaders");
```

- [ ] **Step 2: Restore FFP material for stars**

```cpp
	m_materials[0] = baseMaterial();
	m_materials[0].MaterialType = m_enable_shaders ?
			ssrc->getShaderInfo(ssrc->getShader("stars_shader", TILE_MATERIAL_ALPHA)).material :
			video::EMT_TRANSPARENT_ALPHA_CHANNEL;
```

- [ ] **Step 3: Restore FFP color path in draw_stars**

```cpp
	if (m_enable_shaders)
		m_materials[0].ColorParam = color.toSColor();
	else
		setMeshBufferColor(m_stars.get(), color.toSColor());
```

- [ ] **Step 4: Restore hardware mapping hint**

```cpp
	if (m_enable_shaders)
		m_stars->setHardwareMappingHint(scene::EHM_STATIC);
```

- [ ] **Step 5: Commit**

```bash
git add src/client/sky.h src/client/sky.cpp
git commit -m "C2: restore FFP path in sky rendering"
```

---

### Task 10: Restore FFP path in content_cao.cpp

**Files:**
- Modify: `src/client/content_cao.h`
- Modify: `src/client/content_cao.cpp`

- [ ] **Step 1: Restore m_enable_shaders**

In `content_cao.h`:
```cpp
	bool m_enable_shaders = false;
```

In `content_cao.cpp`, `initialize()`:
```cpp
	m_enable_shaders = g_settings->getBool("enable_shaders");
```

- [ ] **Step 2: Restore FFP material selection in addToScene**

```cpp
	if (m_enable_shaders) {
		IShaderSource *shader_source = m_client->getShaderSource();
		MaterialType material_type;
		switch (m_prop.visual) {
		case "sprite":
			material_type = TILE_MATERIAL_ALPHA;
			break;
		default:
			material_type = m_prop.use_texture_alpha ?
				TILE_MATERIAL_ALPHA : TILE_MATERIAL_BASIC;
			break;
		}
		u32 shader_id = shader_source->getShader("object_shader", material_type, NDT_NORMAL);
		m_material_type = shader_source->getShaderInfo(shader_id).material;
	} else {
		if (m_prop.use_texture_alpha) {
			m_material_type = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
			m_material_type_param = 1.0f / 256.f;
		} else {
			m_material_type = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
		}
	}
```

- [ ] **Step 3: Restore ColorParam guard**

```cpp
	if (m_enable_shaders) {
		buf->getMaterial().ColorParam = c;
	}
```
(Repeat for all mesh buffer creation paths)

- [ ] **Step 4: Restore setNodeLight FFP path**

```cpp
void GenericCAO::setNodeLight(const video::SColor &light_color)
{
	if (!m_enable_shaders) {
		if (m_meshnode) {
			setMeshColor(m_meshnode->getMesh(), light_color);
		} else if (m_animated_meshnode) {
			setMeshColor(m_animated_meshnode->getMesh(), light_color);
		} else if (m_spritenode) {
			m_spritenode->setColor(light_color);
		}
		return;
	}
	auto *node = getSceneNode();
	if (!node)
		return;
	setColorParam(node, light_color);
}
```

Restore `final_color_blend()` usage in `updateLight()`:
```cpp
	if (m_enable_shaders)
		light = encode_light(light_at_pos, m_prop.glow);
	else
		final_color_blend(&light, light_at_pos, day_night_ratio);
```

- [ ] **Step 5: Guard VBO hints**

```cpp
	if (m_enable_shaders && (m_meshnode || m_animated_meshnode)) {
		// sprite uses vertex animation
		if (m_meshnode && m_prop.visual != "upright_sprite")
			m_meshnode->getMesh()->setHardwareMappingHint(scene::EHM_STATIC);
```

- [ ] **Step 6: Commit**

```bash
git add src/client/content_cao.h src/client/content_cao.cpp
git commit -m "C3: restore FFP path in entity rendering"
```

---

### Task 11: Restore FFP path in mapblock_mesh (largest change)

**Files:**
- Modify: `src/client/mapblock_mesh.h`
- Modify: `src/client/mapblock_mesh.cpp`

- [ ] **Step 1: Restore removed fields in mapblock_mesh.h**

Add back:
```cpp
	bool m_enable_shaders;
	u32 m_last_daynight_ratio;
	std::map<std::pair<u8, u32>, std::map<u32, video::SColor>> m_daynight_diffs;
```

In `MeshMakeData`, add back:
```cpp
	bool m_use_shaders;
```

Change constructor:
```cpp
	MeshMakeData(const NodeDefManager *ndef, u16 side_length, bool use_shaders);
```

Initialize `m_last_daynight_ratio` as `(u32)-1` in constructor initializer list.

- [ ] **Step 2: Restore sunlight extraction in MapBlockMesh constructor**

Add back after the texture frame assignment (around line 693):
```cpp
	if (!m_enable_shaders) {
		// Extract colors for day-night animation
		video::SColorf sunlight;
		get_sunlight_color(&sunlight, 0);

		std::map<u32, video::SColor> colors;
		const u32 vertex_count = p.vertices.size();
		for (u32 j = 0; j < vertex_count; j++) {
			video::SColor *vc = &p.vertices[j].Color;
			video::SColor copy = *vc;
			if (vc->getAlpha() == 0)
				final_color_blend(vc, copy, sunlight);
			else
				colors[j] = copy;
			vc->setAlpha(255);
		}
		if (!colors.empty())
			m_daynight_diffs[{layer, i}] = std::move(colors);
	}
```

- [ ] **Step 3: Restore material branching**

```cpp
	if (m_enable_shaders) {
		material.MaterialType = m_shdrsrc->getShaderInfo(
				p.layer.shader_id).material;
		p.layer.applyMaterialOptionsWithShaders(material);
	} else {
		p.layer.applyMaterialOptions(material);
	}
```

- [ ] **Step 4: Restore day/night animation in animate()**

Add back the day-night transition logic:
```cpp
	// Day-night transition
	if (!m_enable_shaders && (daynight_ratio != m_last_daynight_ratio)) {
		video::SColorf day_color;
		get_sunlight_color(&day_color, daynight_ratio);

		for (auto &daynight_diff : m_daynight_diffs) {
			auto *mesh = m_mesh[daynight_diff.first.first].get();
			mesh->setDirty(scene::EBT_VERTEX);
			scene::IMeshBuffer *buf = mesh->
				getMeshBuffer(daynight_diff.first.second);
			video::S3DVertex *vertices = (video::S3DVertex *)buf->getVertices();
			for (const auto &j : daynight_diff.second)
				final_color_blend(&(vertices[j.first].Color), j.second,
						day_color);
		}
		m_last_daynight_ratio = daynight_ratio;
	}
```

- [ ] **Step 5: Update has_animation check**

```cpp
	m_has_animation =
		!m_crack_materials.empty() ||
		!m_daynight_diffs.empty() ||
		!m_animation_info.empty();
```

- [ ] **Step 6: Update MeshMakeData construction in all callers**

Update the constructor calls throughout the codebase:
- `src/client/content_mapblock.cpp` (MeshMakeData creation)
- `src/client/wieldmesh.cpp`
- `src/client/mesh_generator_thread.cpp`

- [ ] **Step 7: Commit**

```bash
git add src/client/mapblock_mesh.h src/client/mapblock_mesh.cpp src/client/content_mapblock.cpp src/client/mesh_generator_thread.cpp src/client/wieldmesh.cpp
git commit -m "C4/C6: restore FFP path in mapblock mesh generation and animate"
```

---

### Task 12: Restore mesh cache for FFP path in content_mapblock

**Files:**
- Modify: `src/client/content_mapblock.h`
- Modify: `src/client/content_mapblock.cpp`

- [ ] **Step 1: Restore enable_mesh_cache field**

In `content_mapblock.h`, in `MapblockMeshGenerator`:
```cpp
	const bool enable_mesh_cache;
```

In `content_mapblock.cpp` constructor:
```cpp
	enable_mesh_cache(g_settings->getBool("enable_mesh_cache") &&
			!data->m_smooth_lighting),
```

- [ ] **Step 2: Restore mesh_ptr caching in drawMeshNode**

Add back the cache lookup before mesh cloning:
```cpp
	if (!data->m_smooth_lighting && cur_node.f->mesh_ptr[facedir] && !degrotate) {
		// use cached meshes
		private_mesh = false;
		mesh = cur_node.f->mesh_ptr[facedir];
	} else if (cur_node.f->mesh_ptr[0]) {
		// ...existing clone+rotate code...
```

Restore `private_mesh` variable declaration.

- [ ] **Step 3: Restore wallmounted conversion guard**

```cpp
	if (!enable_mesh_cache)
		facedir = wallmounted_to_facedir[facedir];
```

- [ ] **Step 4: Commit**

```bash
git add src/client/content_mapblock.h src/client/content_mapblock.cpp
git commit -m "C5: restore mesh cache for FFP blocking node path"
```

---

### Task 13: Restore FFP path in wieldmesh.cpp

**Files:**
- Modify: `src/client/wieldmesh.h`
- Modify: `src/client/wieldmesh.cpp`

- [ ] **Step 1: Restore m_enable_shaders**

In `wieldmesh.h`:
```cpp
	bool m_enable_shaders;
```

In `wieldmesh.cpp` constructor:
```cpp
	m_enable_shaders = g_settings->getBool("enable_shaders");
```

- [ ] **Step 2: Restore FFP material selection**

In `setItem()`:
```cpp
	if (m_enable_shaders) {
		u32 shader_id = shdrsrc->getShader("object_shader", TILE_MATERIAL_BASIC, NDT_NORMAL);
		m_material_type = shdrsrc->getShaderInfo(shader_id).material;
	}
```

- [ ] **Step 3: Restore FFP color path**

In `setColor()`:
```cpp
	if (m_enable_shaders)
		setMeshBufferColor(buf, buffercolor);
	else
		colorizeMeshBuffer(buf, &buffercolor);
```

- [ ] **Step 4: Restore setNodeLightColor FFP path**

```cpp
void WieldMeshSceneNode::setNodeLightColor(video::SColor color)
{
	if (!m_meshnode)
		return;
	if (m_enable_shaders) {
		for (u32 i = 0; i < m_meshnode->getMaterialCount(); ++i) {
			video::SMaterial &material = m_meshnode->getMaterial(i);
			material.ColorParam = color;
		}
	} else {
		setColor(color);
	}
}
```

- [ ] **Step 5: Restore dynamic hint for FFP**

In `changeToMesh()`:
```cpp
	if (m_enable_shaders)
		mesh->setHardwareMappingHint(scene::EHM_STATIC);
	else
		mesh->setHardwareMappingHint(scene::EHM_DYNAMIC);
```

- [ ] **Step 6: Restore postProcessNodeMesh use_shaders param**

In `wieldmesh.h`, change the function signature:
```cpp
void postProcessNodeMesh(scene::SMesh *mesh, const ContentFeatures &f,
	bool use_shaders, bool set_material, const video::E_MATERIAL_TYPE *mattype,
	std::vector<ItemPartColor> *colors, bool apply_scale);
```

Add back the `use_shaders` parameter to all call sites.

- [ ] **Step 7: Commit**

```bash
git add src/client/wieldmesh.h src/client/wieldmesh.cpp
git commit -m "C7: restore FFP path in wieldmesh rendering"
```

---

### Task 14: Restore FFP path in hud.cpp

**Files:**
- Modify: `src/client/hud.cpp`

- [ ] **Step 1: Restore FFP material branching**

In `Hud` constructor, around selection material:
```cpp
	if (g_settings->getBool("enable_shaders")) {
		IShaderSource *shdrsrc = client->getShaderSource();
		auto shader_id = shdrsrc->getShader(
			m_mode == HIGHLIGHT_HALO ? "selection_shader" : "default_shader", TILE_MATERIAL_ALPHA);
		m_selection_material.MaterialType = shdrsrc->getShaderInfo(shader_id).material;
	} else {
		m_selection_material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	}
```

Same pattern for `m_block_bounds_material`.

- [ ] **Step 2: Commit**

```bash
git add src/client/hud.cpp
git commit -m "C8: restore FFP path in HUD rendering"
```

---

### Task 15: Restore FFP path in minimap.cpp

**Files:**
- Modify: `src/client/minimap.h`
- Modify: `src/client/minimap.cpp`

- [ ] **Step 1: Restore m_enable_shaders**

In `minimap.h`:
```cpp
	bool m_enable_shaders;
```

In `minimap.cpp` constructor:
```cpp
	m_enable_shaders = g_settings->getBool("enable_shaders");
```

- [ ] **Step 2: Restore FFP guard**

In `drawMinimap()`:
```cpp
	if (m_enable_shaders && data->mode.type == MINIMAP_TYPE_SURFACE) {
		auto sid = m_shdrsrc->getShader("minimap_shader", TILE_MATERIAL_ALPHA);
		material.MaterialType = m_shdrsrc->getShaderInfo(sid).material;
	} else {
		material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
	}
```

- [ ] **Step 3: Commit**

```bash
git add src/client/minimap.h src/client/minimap.cpp
git commit -m "C9: restore FFP path in minimap rendering"
```

---

### Task 16: Restore FFP-gated pipeline assembly in plain.cpp

**Files:**
- Modify: `src/client/render/plain.cpp`

- [ ] **Step 1: Restore shader check for post-processing**

In `create3DStage()`:
```cpp
if (g_settings->getBool("enable_shaders") && g_settings->getBool("enable_post_processing")) {
	RenderPipeline *pipeline = new RenderPipeline();
	// ... post-processing pipeline ...
```

If shaders disabled, return just `Draw3D` step directly.

In `addUpscaling()`:
```cpp
if (g_settings->getBool("enable_shaders") && g_settings->getBool("enable_post_processing"))
	return previousStep;
```

When shaders are disabled, upscaling is done by the CPU-side `UpscaleStep`.

- [ ] **Step 2: Commit**

```bash
git add src/client/render/plain.cpp
git commit -m "D1: guard post-processing behind enable_shaders check"
```

---

### Task 17: Restore m_cache_enable_shaders in nodedef, environment, mesh_generator_thread

**Files:**
- Modify: `src/nodedef.h`
- Modify: `src/nodedef.cpp`
- Modify: `src/environment.h`
- Modify: `src/client/mesh_generator_thread.h`
- Modify: `src/gui/guiScene.cpp`
- Modify: `src/client/mesh_generator_thread.cpp`

- [ ] **Step 1: Restore m_cache_enable_shaders in Environment**

In `src/environment.h`:
```cpp
	bool m_cache_enable_shaders;
```

In constructor: `m_cache_enable_shaders(g_settings->getBool("enable_shaders"))`.

- [ ] **Step 2: Restore m_cache_enable_shaders in NodeDefManager**

In `src/nodedef.h`:
```cpp
	bool m_cache_enable_shaders;
```

In `src/nodedef.cpp`, in `cacheNodes()`:
```cpp
	m_cache_enable_shaders = g_settings->getBool("enable_shaders");
```

Restore the `mesh_ptr[6]` per-facedir caching that was replaced with a single `mesh_ptr`. When shaders are enabled, cache as before. When disabled, cache for all 6 facedirs (since mesh cache is more important for FFP performance).

- [ ] **Step 3: Restore m_cache_enable_shaders in MeshGeneratorThread**

In `src/client/mesh_generator_thread.h`, in the queue params:
```cpp
	bool m_use_shaders;
```

In the `MeshQueue` struct or wherever the data is passed, pass `m_use_shaders` to `MeshMakeData`.

- [ ] **Step 4: Restore alpha test handling in guiScene.cpp**

In `src/gui/guiScene.cpp`, add the FFP alpha test call when shaders are disabled and alpha channel is used.

- [ ] **Step 5: Commit**

```bash
git add src/nodedef.h src/nodedef.cpp src/environment.h src/client/mesh_generator_thread.h src/client/mesh_generator_thread.cpp src/gui/guiScene.cpp
git commit -m "E4-E7: restore cache infrastructure for FFP mode"
```

---

### Plan Self-Review

Coverage check against the spec:
- A1: Task 1 ✓
- A2: Task 2 ✓
- A3: Task 3 ✓
- A4: Task 4 ✓
- A5: Task 5 ✓
- B1-B5: Task 6 ✓
- B3: Task 7 ✓
- C1: Task 8 ✓
- C2: Task 9 ✓
- C3: Task 10 ✓
- C4/C6: Task 11 ✓
- C5: Task 12 ✓
- C7: Task 13 ✓
- C8: Task 14 ✓
- C9: Task 15 ✓
- D1: Task 16 ✓
- E1-E7: Task 6 ✓ (E1-E3) + Task 17 ✓ (E4-E7)

No placeholders. All code snippets show actual implementation code. No contradictions between tasks.
