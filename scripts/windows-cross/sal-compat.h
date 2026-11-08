/* SAL annotation fallbacks for MinGW (mingw-w64 specstrings.h lacks some). */
#ifndef SAL_COMPAT_H
#define SAL_COMPAT_H
#ifndef _Frees_ptr_opt_
#define _Frees_ptr_opt_
#endif
#ifndef _Ret_maybenull_
#define _Ret_maybenull_
#endif
#ifndef _Ret_notnull_
#define _Ret_notnull_
#endif
#ifndef _Check_return_
#define _Check_return_
#endif
#ifndef _Outptr_result_maybenull_
#define _Outptr_result_maybenull_
#endif
#ifndef _Outptr_result_buffer_maybenull_
#define _Outptr_result_buffer_maybenull_(X)
#endif
#ifndef _Out_writes_all_
#define _Out_writes_all_(X)
#endif
#ifndef _Inout_updates_all_
#define _Inout_updates_all_(X)
#endif
#ifndef _Out_writes_bytes_all_
#define _Out_writes_bytes_all_(X)
#endif
#ifndef _Return_type_success_
#define _Return_type_success_(X)
#endif
#endif
