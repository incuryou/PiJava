# deko3d API contract used by host tests

`deko3d.h` is an unmodified public API header from the restored devkitPro SDK.
Copyright and permissive license notice remain in the file. Production builds
use the real SDK header and library, never the mock implementation.

Upstream: https://github.com/devkitPro/deko3d/blob/master/include/deko3d.h

The mock tests check this project's ownership/ordering, error cleanup and pixel
composition. They do not emulate Horizon's compositor, GPU cache coherence,
GPU command encodings, performance, or deko3d fatal driver callbacks.
