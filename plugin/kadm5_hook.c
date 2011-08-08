/*
 * MIt kadm5_hook API
 *
 * This is the glue required to connect kadmin hook module to the
 * API for the krb5-sync module.  It is based on the kadm5_hook
 * interface released with MIT Kerberos 1.9 which was based on a
 * preliminary proposal for
 * the Heimdal hook API.
 *
 * Written by Russ Allbery <rra@stanford.edu> and updated by Sam
 * Hartman <hartmans@painless-security.com>
 * Copyright 2010 Board of Trustees, Leland Stanford Jr. University
 * copyright 2010, the Massachusetts Institute of Technology
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <errno.h>
#include <kadm5/admin.h>
#ifdef HAVE_KADM5_KADM5_ERR_H
# include <kadm5/kadm5_err.h>
#endif
#include <krb5.h>
#ifdef HAVE_KRB5_KADM5_HOOK_PLUGIN_H
#include <krb5/kadm5_hook_plugin.h>

#include <plugin/internal.h>
#include <util/macros.h>


/*
 * Initialize the plugin.  Calls the pwupdate_init() function and returns the
 * resulting data object.
 */
static kadm5_ret_t
init(krb5_context ctx, kadm5_hook_modinfo **data)
{
    krb5_error_code code = 0;

    if (pwupdate_init(ctx, (void **) data) != 0)
        code = errno;
    return code;
}


/*
 * Shut down the object, freeing any internal resources.
 */
static void
fini(krb5_context ctx UNUSED, kadm5_hook_modinfo *data)
{
    pwupdate_close(data);
}


/*
 * Handle a password change.
 */
static kadm5_ret_t
chpass(krb5_context ctx, kadm5_hook_modinfo *data, int stage,
       krb5_principal princ,
       krb5_boolean keepoldUNUSED,
                                 int n_ks_tuple UNUSED,
                          krb5_key_salt_tuple *ks_tuple UNUSED,
       const char *password)
{
    char error[BUFSIZ];
    size_t length;
    int status = 0;

    length = strlen(password);
    if (stage == KADM5_HOOK_STAGE_PRECOMMIT)
        status = pwupdate_precommit_password(data, princ, password, length,
                                             error, sizeof(error));
    else if (stage == KADM5_HOOK_STAGE_POSTCOMMIT)
        status = pwupdate_postcommit_password(data, princ, password, length,
                                              error, sizeof(error));
    if (status == 0)
        return 0;
    else {
        krb5_set_error_message(ctx, KADM5_FAILURE,
                               "cannot synchronize password: %s", error);
        return KADM5_FAILURE;
    }
}


/*
 * Handle a principal creation.
 *
 * We only care about synchronizing the password, so we just call the same
 * hooks as we did for a password change.
 */
static kadm5_ret_t
create(krb5_context ctx, kadm5_hook_modinfo *data, int stage,
       kadm5_principal_ent_t entry, long mask UNUSED,
                                 int n_ks_tuple UNUSED,
                          krb5_key_salt_tuple *ks_tuple UNUSED,
       const char *password)
{
    char error[BUFSIZ];
    size_t length;
    int status = 0;

    length = strlen(password);
    if (stage == KADM5_HOOK_STAGE_PRECOMMIT)
        status = pwupdate_precommit_password(data, entry->principal, password,
                                             length, error, sizeof(error));
    else if (stage == KADM5_HOOK_STAGE_POSTCOMMIT)
        status = pwupdate_postcommit_password(data, entry->principal,
                                              password, length, error,
                                              sizeof(error));
    if (status == 0)
        return 0;
    else {
        krb5_set_error_message(ctx, KADM5_FAILURE,
                               "cannot synchronize password: %s", error);
        return KADM5_FAILURE;
    }
}


/*
 * Handle a principal modification.
 *
 * We only care about changes to the DISALLOW_ALL_TIX flag, and we only
 * support status postcommit.  Check whether that's what's being changed and
 * call the appropriate hook.
 */
static kadm5_ret_t
modify(krb5_context ctx, kadm5_hook_modinfo *data, int stage,
       kadm5_principal_ent_t entry, long mask)
{
    char error[BUFSIZ];
    int enabled, status;

    if (mask & KADM5_ATTRIBUTES && stage == KADM5_HOOK_STAGE_POSTCOMMIT) {
        enabled = !(entry->attributes & KRB5_KDB_DISALLOW_ALL_TIX);
        status = pwupdate_postcommit_status(data, entry->principal, enabled,
                                            error, sizeof(error));
        if (status == 0)
            return 0;
        else {
            krb5_set_error_message(ctx, KADM5_FAILURE,
                                   "cannot synchronize status: %s", error);
            return KADM5_FAILURE;
        }
    }
    return 0;
}

krb5_error_code
kadm5_hook_test_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable);

krb5_error_code
kadm5_hook_krb5_sync_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable)
{
    kadm5_hook_vftable_1 *vt = (kadm5_hook_vftable_1 *) vtable;
    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;

    vt->name = "krb5_sync";
    vt->init = init;
    vt->fini = fini;
    vt->chpass = chpass;
    vt->create = create;
    vt->modify = modify;
    return 0;
}


#endif /*KADM5_HOOK_PLUGIN_H*/