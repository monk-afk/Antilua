# OpenGL 1.4 Compatibility — Summary of Changes

Branch: `opengl-14-compat` (19 commits on top of master)

## Overview

Restored the ability to run Minetest (Luanti) with OpenGL 1.4 core via an `enable_shaders` setting toggle. When shaders are disabled, the game uses the legacy fixed-function pipeline (FFP) through the EDT_OPENGL driver instead of the GL3 driver. All rendering paths that were unified to use only GLSL shaders now have a FFP fallback.

## Architecture

```
                  ┌──────────────────────────┐
                  │   enable_shaders=false    │
                  │   → EDT_OPENGL (legacy)   │
                  │   → COpenGLDriver + FFP   │
                  │   → No GLSL at all        │
                  └──────────────────────────┘

vs.

                  ┌──────────────────────────┐
                  │   enable_shaders=true     │
                  │   → EDT_OPENGL3 (modern)  │
                  │   → COpenGL3DriverBase    │
                  │   → GLSL shaders          │
                  └──────────────────────────┘
```

## Driver Layer Changes

### A1 — GL 1.4 Context Request
- **`irr/src/CIrrDeviceSDL.cpp`**: EDT_OPENGL now requests GL 1.4 context (was 2.1), falls back to 2.1 on failure

### A2 — VBOs Optional
- **`irr/include/IVideoDriver.h`**: Added `virtual void setVBOEnabled(bool)` with default no-op
- **`irr/src/COpenGLDriver.h`**: Override that sets `m_vbo_enabled`
- **`irr/src/COpenGLDriver.cpp`**: `createHardwareBuffer()`, `updateVertexHardwareBuffer()`, `updateIndexHardwareBuffer()` all check `m_vbo_enabled` before creating/using VBOs

### A3 — FBO Availability Check
- **`irr/src/COpenGLDriver.h/cpp`**: Added `isFBOAvailable()` method

### A4 — GL_MODULATE Fallback for Texture Env Combine
- **`irr/src/COpenGLMaterialRenderer.h`**: All `#ifdef GL_ARB_texture_env_combine` → runtime `queryOpenGLFeature()` check. Falls back to `GL_MODULATE` when `ARB_texture_env_combine` is unavailable

### A5 — Alpha Test Support
- Already present in the legacy `COpenGLCacheHandler.h/cpp` — `setAlphaFunc()`, `setAlphaTest()` methods exist and are used by FFP material renderers

## Client Rendering Changes

### B — Shader System

- **`src/defaultsettings.cpp`**: Added `enable_shaders = true` default
- **`builtin/settingtypes.txt`**: Added `enable_shaders` and `enable_mesh_cache` setting definitions
- **`src/client/shader.cpp`**: `ShaderSource` checks `m_enabled` — when false, returns base FFP material types from `getShader()`/`generateShader()`, skips GLSL compilation, destructor/rebuild are no-ops
- **`src/client/renderingengine.cpp`**: When `enable_shaders = false`, the driver list prefers EDT_OPENGL over EDT_OPENGL3
- **`builtin/common/settings/`**: Restored `shader_warning_component.lua` and shader requirement checks in `dlg_settings.lua`

### C — Per-Object FFP Rendering Paths

**C1 — Clouds (`src/client/clouds.cpp/h`):**
- Added `m_enable_shaders`
- FFP: `EMT_TRANSPARENT_ALPHA_CHANNEL` material, pre-baked vertex colors, no `ColorParam`

**C2 — Sky (`src/client/sky.cpp/h`):**
- Added `m_enable_shaders`
- FFP: `EMT_TRANSPARENT_ALPHA_CHANNEL` for stars, `setMeshBufferColor()` instead of `ColorParam`

**C3 — Entities (`src/client/content_cao.cpp/h`):**
- Added `m_enable_shaders`, `m_material_type_param`
- FFP material selection: `EMT_TRANSPARENT_ALPHA_CHANNEL` or `EMT_TRANSPARENT_ALPHA_CHANNEL_REF`
- `setNodeLight()` FFP path: `setMeshColor()` instead of `setColorParam()`; skips zero-alpha initial colors
- `updateLight()` FFP path: `final_color_blend()` instead of `encode_light()`

**C4 — Mapblock Mesh (`src/client/mapblock_mesh.cpp/h`):**
- Restored `MeshMakeData::m_use_shaders`, `MapBlockMesh::m_enable_shaders`, `m_daynight_diffs`, `m_last_daynight_ratio`
- Constructor: extract sunlight from vertex alpha, bake into RGB, store diffs for day/night animation
- `animate()`: day/night vertex color transition via `final_color_blend()`
- `m_has_animation` includes `m_daynight_diffs`

**C5 — Mesh Cache (`src/client/content_mapblock.cpp/h`):**
- Restored `enable_mesh_cache` for non-smooth-lighted opaque nodes

**C6 — MeshMakeData constructor** updated to 3 params (+ `use_shaders`)

**C7 — Wieldmesh (`src/client/wieldmesh.cpp/h`):**
- Added `m_enable_shaders`
- FFP: `colorizeMeshBuffer()` instead of `setMeshBufferColor()`, `EHM_DYNAMIC` hint
- Added `colorizeMeshBuffer(buf, color)` overload to `mesh.cpp/h`

**C8 — HUD (`src/client/hud.cpp`):**
- Selection/block bounds material: `EMT_TRANSPARENT_ALPHA_CHANNEL` when shaders disabled

**C9 — Minimap (`src/client/minimap.cpp/h`):**
- Added `m_enable_shaders`; surface mode uses `EMT_TRANSPARENT_ALPHA_CHANNEL_REF`

### D — Pipeline Assembly

- **`src/client/render/plain.cpp`**: Post-processing pipeline guarded behind `enable_shaders && enable_post_processing`

### E — Infrastructure

- **`src/environment.h`**: Restored `m_cache_enable_shaders`
- **`src/client/mesh_generator_thread.h/cpp`**: Restored `m_use_shaders`, `m_cache_enable_shaders`

## FFP Material Type Mapping

The FFP variant of `applyMaterialOptions()` in `tile.cpp` maps each `TILE_MATERIAL_*` to the correct FFP material:

| Tile Material Type | FFP Material | Behavior |
|---|---|---|
| `TILE_MATERIAL_BASIC` | `EMT_TRANSPARENT_ALPHA_CHANNEL_REF` | Alpha test (clip < 0.5) |
| `TILE_MATERIAL_WAVING_LEAVES` | `EMT_TRANSPARENT_ALPHA_CHANNEL_REF` | Alpha test |
| `TILE_MATERIAL_WAVING_PLANTS` | `EMT_TRANSPARENT_ALPHA_CHANNEL_REF` | Alpha test |
| `TILE_MATERIAL_WAVING_LIQUID_BASIC` | `EMT_TRANSPARENT_ALPHA_CHANNEL_REF` | Alpha test |
| `TILE_MATERIAL_LIQUID_OPAQUE` | `EMT_TRANSPARENT_ALPHA_CHANNEL_REF` | Alpha test |
| `TILE_MATERIAL_WAVING_LIQUID_OPAQUE` | `EMT_TRANSPARENT_ALPHA_CHANNEL_REF` | Alpha test |
| `TILE_MATERIAL_OPAQUE` | `EMT_SOLID` | Opaque |
| `TILE_MATERIAL_PLAIN` | `EMT_SOLID` | Opaque |
| `TILE_MATERIAL_ALPHA` | `EMT_TRANSPARENT_ALPHA_CHANNEL` | Alpha blend |
| `TILE_MATERIAL_LIQUID_TRANSPARENT` | `EMT_TRANSPARENT_ALPHA_CHANNEL` | Alpha blend |
| `TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT` | `EMT_TRANSPARENT_ALPHA_CHANNEL` | Alpha blend |
| `TILE_MATERIAL_PLAIN_ALPHA` | `EMT_TRANSPARENT_ALPHA_CHANNEL` | Alpha blend |

## Known Limitations (when `enable_shaders = false`)

- No GLSL shaders at all (by design)
- No post-processing (bloom, FXAA, volumetric light, auto-exposure, tone mapping)
- No dynamic shadows
- No waving animation (waving materials treated as static)
- Day/night cycle uses CPU vertex color updates (slower than shader path)
- VBOs disabled (uses client-side vertex arrays)
- FBOs unavailable (no render-to-texture)
- Mesh cache enabled only for opaque, non-smooth-lighted nodes
- Some visual effects may look different (lighting is baked into vertex colors)
