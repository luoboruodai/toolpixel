# Notice

- The Adobe After Effects SDK is **not included**. Configure `AE_SDK_ROOT` with an SDK you are licensed to use. Adobe's headers, resources and tools remain subject to their own terms.
- ToonPixel SmartFX is a native RGB/Alpha approximation effect. It does not reconstruct a 3D scene or consume a true geometry G-buffer.
- The macOS bundle is ad-hoc signed for development/testing, **not notarized**. The Windows `.aex` has **no Authenticode signature**.
- The Windows attachment is a cross-built preview and has not been host-tested in Windows After Effects. Test on a disposable project before deployment.
- Basic Latin 8×8 glyph data has a public-domain upstream notice; see `THIRD_PARTY_NOTICES.md` and `src/ae/ToonPixelGlyphs.h`.
- Reference plug-in names are used only to explain local research direction. No third-party plug-in binary, private code, proprietary palette or private video asset is redistributed.
