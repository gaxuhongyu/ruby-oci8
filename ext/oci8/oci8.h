/*
  oci8.h - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>
*/
#ifndef _RUBY_OCI_H_
#define _RUBY_OCI_H_ 1

#include "ruby.h"
#include "rubyio.h"
#include "intern.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <oci.h>
#ifdef __cplusplus
}
#endif
#include "extconf.h"

#define DEBUG_CORE_FILE 1
#define OCI8_DEBUG 1
#ifdef OCI8_DEBUG
#define ASSERT(v) if (!(v)) { rb_bug("%s:%d: " #v, __FILE__, __LINE__); }
#define ASSERT_(v) if (!(v)) { abort(); }
#else
#define ASSERT(v)
#endif

#define IS_OCI_ERROR(v) (((v) != OCI_SUCCESS) && ((v) != OCI_SUCCESS_WITH_INFO))

#define rb_define_method_nodoc rb_define_method

typedef struct oci8_base_class oci8_base_class_t;
typedef struct oci8_bind_class oci8_bind_class_t;

typedef struct oci8_base oci8_base_t;
typedef struct oci8_bind oci8_bind_t;

struct oci8_base_class {
    void (*mark)(oci8_base_t *base);
    void (*free)(oci8_base_t *base);
    size_t size;
};

struct oci8_bind_class {
    oci8_base_class_t base;
    VALUE (*get)(oci8_bind_t *bh);
    void (*set)(oci8_bind_t *bh, VALUE val);
    void (*init)(oci8_bind_t *bh, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale);
    ub2 dty;
};

struct oci8_base {
    ub4 type;
    dvoid *hp;
    VALUE self;
    oci8_base_class_t *klass;
};

struct oci8_bind {
    oci8_base_t base;
    oci8_bind_t *next;
    oci8_bind_t *prev;
    void *valuep;
    sb4 value_sz;
    ub2 rlen;
    sb2 ind;
};

enum logon_type_t {T_IMPLICIT, T_EXPLICIT};

typedef struct  {
    oci8_base_t base;
    VALUE executing_thread;
    enum logon_type_t logon_type;
    OCISession *authhp;
    OCIServer *srvhp;
} oci8_svcctx_t;

#define Check_Handle(obj, klass, hp) do { \
    if (!rb_obj_is_kind_of(obj, klass)) { \
        rb_raise(rb_eTypeError, "invalid argument %s (expect %s)", rb_class2name(CLASS_OF(obj)), rb_class2name(klass)); \
    } \
    Data_Get_Struct(obj, oci8_base_t, hp); \
} while (0)

#define Check_Object(obj, klass) do {\
  if (!rb_obj_is_kind_of(obj, klass)) { \
    rb_raise(rb_eTypeError, "invalid argument %s (expect %s)", rb_class2name(CLASS_OF(obj)), rb_class2name(klass)); \
  } \
} while (0)

#define Get_Int_With_Default(argc, pos, vval, cval, def) do { \
  if (argc >= pos) { \
    Check_Type(vval, T_FIXNUM); \
    cval = FIX2INT(vval); \
  } else { \
    cval = def; \
  } \
} while (0)

/* use for local call */
#define oci_lc(rv) do { \
    sword __rv = (rv); \
    if (__rv != OCI_SUCCESS) { \
        oci8_raise(oci8_errhp, __rv, NULL); \
    } \
} while(0)

#ifndef HAVE_OCIRESET
#define OCIReset(svchp, errhp) do {} while(0)
#endif

#define NB_STATE_NOT_EXECUTING INT2FIX(0)
#define NB_STATE_CANCELING     INT2FIX(1)
/* remote call without check */
#define oci_rc2(rv, svcctx, func) do { \
    struct timeval __time; \
    sword __r; \
    if (svcctx->executing_thread != NB_STATE_NOT_EXECUTING) { \
        rb_raise(rb_eRuntimeError /* FIXME */, "executing in another thread"); \
    } \
    __time.tv_sec = 0; \
    __time.tv_usec = 100000; \
    svcctx->executing_thread = rb_thread_current(); \
    while ((__r = (func)) == OCI_STILL_EXECUTING) { \
        rb_thread_wait_for(__time); \
        if (svcctx->executing_thread == NB_STATE_CANCELING) { \
            svcctx->executing_thread = NB_STATE_NOT_EXECUTING; \
            OCIReset(svcctx->base.hp, oci8_errhp); \
            rb_raise(eOCIBreak, "Canceled by user request."); \
        } \
        if (__time.tv_usec < 500000) \
        __time.tv_usec <<= 1; \
    } \
    if (__r == OCI_ERROR) { \
       ub4 errcode; \
       OraText errmsg[1]; \
       OCIErrorGet(oci8_errhp, 1, NULL, &errcode, errmsg, sizeof(errmsg), OCI_HTYPE_ERROR); \
       if (errcode == 1013) { \
            svcctx->executing_thread = NB_STATE_NOT_EXECUTING; \
            OCIReset(svcctx->base.hp, oci8_errhp); \
            rb_raise(eOCIBreak, "Canceled by user request."); \
       } \
    } \
    svcctx->executing_thread = NB_STATE_NOT_EXECUTING; \
    (rv) = __r; \
} while (0)

/* remote call */
#define oci_rc(svcctx, func) do { \
    sword __rv; \
    oci_rc2(__rv, svcctx, func); \
    if (__rv != OCI_SUCCESS) { \
        oci8_raise(oci8_errhp, __rv, NULL); \
    } \
} while (0)


#if SIZEOF_LONG > 4
#define UB4_TO_NUM INT2FIX
#else
#define UB4_TO_NUM UINT2NUM
#endif

/* dangerous macros */
#define CHECK_STRING(obj) if (!NIL_P(obj)) { StringValue(obj); }
#define TO_STRING_PTR(obj) (NIL_P(obj) ? NULL : RSTRING(obj)->ptr)
#define TO_STRING_LEN(obj) (NIL_P(obj) ? 0 : RSTRING(obj)->len)

/* env.c */
extern OCIEnv *oci8_envhp;
extern OCIError *oci8_errhp;
void Init_oci8_env(void);

/* oci8lib.c */
extern ID oci8_id_new;
void oci8_base_free(oci8_base_t *base);
VALUE oci8_define_class(const char *name, oci8_base_class_t *klass);
VALUE oci8_define_class_under(VALUE outer, const char *name, oci8_base_class_t *klass);
VALUE oci8_define_bind_class(const char *name, oci8_bind_class_t *oci8_bind_class);
extern oci8_base_class_t oci8_base_class;

/* error.c */
extern VALUE eOCIException;
extern VALUE eOCIBreak;
void Init_oci8_error(void);
NORETURN(void oci8_raise(OCIError *, sword status, OCIStmt *));
NORETURN(void oci8_env_raise(OCIEnv *, sword status));

/* oci8.c */
VALUE Init_oci8(void);
oci8_svcctx_t *oci8_get_svcctx(VALUE obj);
OCISvcCtx *oci8_get_oci_svcctx(VALUE obj);
OCISession *oci8_get_oci_session(VALUE obj);
#define TO_SVCCTX oci8_get_oci_svcctx
#define TO_SESSION oci8_get_oci_session

/* stmt.c */
void Init_oci8_stmt(void);

/* bind.c */
typedef struct {
    oci8_bind_t bind;
    void *hp;
    VALUE obj;
} oci8_bind_handle_t;
void oci8_bind_free(oci8_base_t *base);
void oci8_bind_handle_mark(oci8_base_t *base);
VALUE oci8_bind_handle_get(oci8_bind_t *bind);
void Init_oci8_bind(VALUE cOCIBind);
oci8_bind_t *oci8_get_bind(VALUE obj);

/* rowid.c */
void Init_oci8_rowid(void);
VALUE oci8_get_rowid_attr(oci8_base_t *base, ub4 attrtype);

/* param.c */
void Init_oci8_param(void);
VALUE oci8_param_create(OCIParam *parmhp, OCIError *errhp);

/* lob.c */
void Init_oci8_lob(VALUE cOCI8);

/* oradate.c */
void Init_ora_date(void);

/* oranumber.c */
void Init_ora_number(void);

/* ocinumber.c */
void Init_oci_number(VALUE mOCI);
OCINumber *oci8_get_ocinumber(VALUE num);

/* ocidatetim.c */
void Init_oci_datetime(void);

/* attr.c */
VALUE oci8_get_sb1_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_ub2_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_sb2_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_ub4_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_string_attr(oci8_base_t *base, ub4 attrtype);

#define _D_ fprintf(stderr, "%s:%d - %s\n", __FILE__, __LINE__, __FUNCTION__)
#endif
