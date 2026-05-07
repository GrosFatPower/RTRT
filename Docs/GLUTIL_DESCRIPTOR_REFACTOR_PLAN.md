# GLUtil Descriptor Refactor Plan

## Summary

Refactor `GLUtil` into a descriptor-based helper layer while preserving renderer behavior, texture formats, texture slots, filtering, wrapping, framebuffer attachments, resize behavior, and rendering outputs.

## Key Changes

- Add plain project-style descriptors in `GLUtil.h`:
  - `GLTextureDesc`
  - `GLBufferDesc`
  - `GLTextureBufferDesc`
  - `GLVertexAttribDesc`
  - `GLFrameBufferAttachmentDesc`
  - `GLFrameBufferDesc`
- Make `GLTexture` easier to default-construct by giving `_Target` and `_Slot` defaults and removing `const` from `_Slot`; keep aggregate initialization order compatible.
- Change `GLFrameBuffer` so `_Tex` is a non-owning `std::vector<GLTexture*>`; `DeleteFBO` deletes only the framebuffer handle and clears the attachment list.
- Add descriptor APIs for texture creation/upload/resize, generic buffers, texture buffers, framebuffer creation, and mesh buffer creation.
- Keep compatibility wrappers for old names such as `GenTexture`, `LoadTexture`, `InitializeTBO`, and tuple-based `CreateMeshBuffers`.

## Implementation Changes

- Migrate `DeferredRenderer` framebuffer and texture setup to descriptors while preserving all formats, slots, attachment order, filters, wraps, mipmap behavior, and pass order.
- Migrate `PathTracer` away from FBO-owned texture copies by adding owned texture members/arrays and storing non-owning pointers in each FBO.
- Migrate `SoftwareRasterizer` similarly for render target and temporary output FBO textures.
- Migrate `Test5` material preview texture uploads to the descriptor path or descriptor-backed compatibility wrappers.
- Update all `ActivateTextures`, `ResizeFBO`, and delete paths so FBOs no longer imply texture ownership.

## Test Plan

- Build the existing CMake target and treat warnings/errors as regressions.
- Smoke test `DeferredRenderer`, `PathTracer`, and `SoftwareRasterizer`.
- In deferred mode, verify G-buffer debug modes, shadows, SSAO, SSAO blur, specular IBL, transparency, resize, and render-to-file.
- In path tracer mode, verify render target, tile/low-res targets, accumulation, denoise texture, resize, and render-to-file.
- In software rasterizer mode, verify render-to-screen, resize, material preview textures, and render-to-file.
- Require framebuffer completeness checks to pass for every descriptor-created FBO.

## Assumptions

- `GLUtil` remains header-only.
- Behavior preservation means no intentional changes to GL formats, shader bindings, texture units, filtering, wrapping, mipmap generation, draw buffers, or render pass order.
- Descriptor APIs become preferred, while old helper names remain as forwarding shims.
- FBOs become non-owning attachment registries; explicit `GLTexture` members or local textures own texture lifetime.
