// vm_migrate.h — VMStorage access macros (P2.1 final: single code path; the P2.1-c J2ME_MIGRATE_*
// A/B switches and their legacy declarations were removed).
//   VMS_DIAG(g_p1)      -> ::j2me_vmstate::acc::diag_g_p1()       (returns T&, routed through vm_state())
// The accessor is only *declared* at the old declaration site (where T is complete) and is defined
// after the single VMStorage object at the end of the translation unit (vm_storage.h).
#ifndef J2ME_VM_MIGRATE_H
#define J2ME_VM_MIGRATE_H

#define VMS_DIAG(n) (::j2me_vmstate::acc::diag_##n())
#define VMS_DIAG_ACCESSOR(n, ...) namespace j2me_vmstate { namespace acc { static inline __VA_ARGS__& diag_##n() noexcept; } }

#define VMS_NATIVE(n) (::j2me_vmstate::acc::natives_##n())
#define VMS_NATIVE_ACCESSOR(n, ...) namespace j2me_vmstate { namespace acc { static inline __VA_ARGS__& natives_##n() noexcept; } }
#define VMS_CL(n) (::j2me_vmstate::acc::classes_##n())
#define VMS_CL_ACCESSOR(n, ...) namespace j2me_vmstate { namespace acc { static inline __VA_ARGS__& classes_##n() noexcept; } }

#define VMS_EXEC(n) (::j2me_vmstate::acc::exec_##n())
#define VMS_EXEC_ACCESSOR(n, ...) namespace j2me_vmstate { namespace acc { static inline __VA_ARGS__& exec_##n() noexcept; } }
#define VMS_HEAP(n) (::j2me_vmstate::acc::heap_##n())
#define VMS_HEAP_ACCESSOR(n, ...) namespace j2me_vmstate { namespace acc { static inline __VA_ARGS__& heap_##n() noexcept; } }
#define VMS_PLAT(n) (::j2me_vmstate::acc::platform_##n())
#define VMS_PLAT_ACCESSOR(n, ...) namespace j2me_vmstate { namespace acc { static inline __VA_ARGS__& platform_##n() noexcept; } }
// Function-local static owned by the platform: a local
// reference with the SAME name bound to the storage slot (so the function body is not edited).
#define VMS_PLAT_LOCAL(local, slot, ...) auto& local = ::j2me_vmstate::acc::platform_##slot();
#define VMS_PLAT_LOCAL3(l1, s1, l2, s2, l3, s3, ...) VMS_PLAT_LOCAL(l1, s1) VMS_PLAT_LOCAL(l2, s2) VMS_PLAT_LOCAL(l3, s3)
#define VMS_RS(n) (::j2me_vmstate::acc::render_##n())
#define VMS_RS_ACCESSOR(n, ...) namespace j2me_vmstate { namespace acc { static inline __VA_ARGS__& render_##n() noexcept; } }
// Namespace-scope member of a render header (m3g_*::stats, paint_hotspots241::*): the old name becomes a
// reference bound to the storage slot; the binding function is declared in that namespace and defined in
// vm_access_impl.h.
#define VMS_RS_NSREF(fn, name, ...) static __VA_ARGS__& fn() noexcept; static __VA_ARGS__& name = fn();
#define VMS_RS_LOCAL(local, slot, ...) auto& local = ::j2me_vmstate::acc::render_##slot();

// Facade field count adjustment (a migrated module keeps ONE reference instead of N per-field refs).
#define VMS_ADJ_D_ (6 - 1)
#define VMS_ADJ_N_ (4 - 1)
#define VMS_ADJ_C_ (6 - 1)
#define VMS_ADJ_H_ (13 - 1)
#define VMS_ADJ_E_ (68 - 1)
#define VMS_FACADE_FIELDS_ADJ - VMS_ADJ_D_ - VMS_ADJ_N_ - VMS_ADJ_C_ - VMS_ADJ_H_ - VMS_ADJ_E_
// c5: the platform module keeps ONE reference (desktop 18 fields, switch 51; kVer/g_host_skin are init-only,
// stay plain globals and leave the facade).
#define VMS_FACADE_PLAT_ADJ_DESKTOP - (18 - 1)
#define VMS_FACADE_PLAT_ADJ_SWITCH - (51 - 1)
// c6: the render module keeps ONE reference (84 facade fields: 80 mutable + g_frame_sig's 2 former unused siblings,
// former g_static_screen (all 3 deleted in P2.1 final), font8x8_basic; the 4 init-only ones stay plain globals and leave the facade).
#define VMS_FACADE_RS_ADJ - (84 - 1)
#endif
