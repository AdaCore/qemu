/*
 * QEMU TPM Backend
 *
 * Copyright IBM, Corp. 2013
 *
 * Authors:
 *  Stefan Berger   <stefanb@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 * Based on backends/rng.c by Anthony Liguori
 */

#include "qemu/osdep.h"
#include "sysemu/tpm_backend.h"
#include "qapi/error.h"
#include "qapi/qmp/qerror.h"
#include "sysemu/tpm.h"
#include "qemu/thread.h"
#include "qemu/main-loop.h"

static void tpm_backend_request_completed_bh(void *opaque)
{
    TPMBackend *s = TPM_BACKEND(opaque);
    TPMIfClass *tic = TPM_IF_GET_CLASS(s->tpmif);

    tic->request_completed(s->tpmif);
}

static void tpm_backend_worker_thread(gpointer data, gpointer user_data)
{
    TPMBackend *s = TPM_BACKEND(user_data);
    TPMBackendClass *k = TPM_BACKEND_GET_CLASS(s);

    k->handle_request(s, (TPMBackendCmd *)data);

    qemu_bh_schedule(s->bh);
}

static void tpm_backend_thread_end(TPMBackend *s)
{
    if (s->thread_pool) {
        g_thread_pool_free(s->thread_pool, FALSE, TRUE);
        s->thread_pool = NULL;
    }
}

enum TpmType tpm_backend_get_type(TPMBackend *s)
{
    TPMBackendClass *k = TPM_BACKEND_GET_CLASS(s);

    return k->type;
}

int tpm_backend_init(TPMBackend *s, TPMIf *tpmif, Error **errp)
{
    if (s->tpmif) {
        error_setg(errp, "TPM backend '%s' is already initialized", s->id);
        return -1;
    }

    s->tpmif = tpmif;
    object_ref(OBJECT(tpmif));

    s->had_startup_error = false;

    return 0;
}

int tpm_backend_startup_tpm(TPMBackend *s, size_t buffersize)
{
    int res = 0;
    TPMBackendClass *k = TPM_BACKEND_GET_CLASS(s);

    /* terminate a running TPM */
    tpm_backend_thread_end(s);

    s->thread_pool = g_thread_pool_new(tpm_backend_worker_thread, s, 1, TRUE,
                                       NULL);

    res = k->startup_tpm ? k->startup_tpm(s, buffersize) : 0;

    s->had_startup_error = (res != 0);

    return res;
}

bool tpm_backend_had_startup_error(TPMBackend *s)
{
    return s->had_startup_error;
}

void tpm_backend_deliver_request(TPMBackend *s, TPMBackendCmd *cmd)
{
    g_thread_pool_push(s->thread_pool, cmd, NULL);
}

void tpm_backend_reset(TPMBackend *s)
{
    TPMBackendClass *k = TPM_BACKEND_GET_CLASS(s);

    if (k->reset) {
        k->reset(s);
    }

    tpm_backend_thread_end(s);

    s->had_startup_error = false;
}

void tpm_backend_cancel_cmd(TPMBackend *s)
{
    TPMBackendClass *k = TPM_BACKEND_GET_CLASS(s);

    k->cancel_cmd(s);
}

bool tpm_backend_get_tpm_established_flag(TPMBackend *s)
{
    TPMBackendClass *k = TPM_BACKEND_GET_CLASS(s);

    return k->get_tpm_established_flag ?
           k->get_tpm_established_flag(s) : false;
}

int tpm_backend_reset_tpm_established_flag(TPMBackend *s, uint8_t locty)
{
    TPMBackendClass *k = TPM_BACKEND_GET_CLASS(s);

    return k->reset_tpm_established_flag ?
           k->reset_tpm_established_flag(s, locty) : 0;
}

TPMVersion tpm_backend_get_tpm_version(TPMBackend *s)
{
    TPMBackendClass *k = TPM_BACKEND_GET_CLASS(s);

    return k->get_tpm_version(s);
}

size_t tpm_backend_get_buffer_size(TPMBackend *s)
{
    TPMBackendClass *k = TPM_BACKEND_GET_CLASS(s);

    return k->get_buffer_size(s);
}

TPMInfo *tpm_backend_query_tpm(TPMBackend *s)
{
    TPMInfo *info = g_new0(TPMInfo, 1);
    TPMBackendClass *k = TPM_BACKEND_GET_CLASS(s);
    TPMIfClass *tic = TPM_IF_GET_CLASS(s->tpmif);

    info->id = g_strdup(s->id);
    info->model = tic->model;
    info->options = k->get_tpm_options(s);

    return info;
}

static void tpm_backend_instance_init(Object *obj)
{
    TPMBackend *s = TPM_BACKEND(obj);

    s->bh = qemu_bh_new(tpm_backend_request_completed_bh, s);
}

static void tpm_backend_instance_finalize(Object *obj)
{
    TPMBackend *s = TPM_BACKEND(obj);

    object_unref(OBJECT(s->tpmif));
    g_free(s->id);
    tpm_backend_thread_end(s);
    qemu_bh_delete(s->bh);
}

static const TypeInfo tpm_backend_info = {
    .name = TYPE_TPM_BACKEND,
    .parent = TYPE_OBJECT,
    .instance_size = sizeof(TPMBackend),
    .instance_init = tpm_backend_instance_init,
    .instance_finalize = tpm_backend_instance_finalize,
    .class_size = sizeof(TPMBackendClass),
    .abstract = true,
};

static const TypeInfo tpm_if_info = {
    .name = TYPE_TPM_IF,
    .parent = TYPE_INTERFACE,
    .class_size = sizeof(TPMIfClass),
};

static void register_types(void)
{
    type_register_static(&tpm_backend_info);
    type_register_static(&tpm_if_info);
}

type_init(register_types);
