/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * Defines the exported symbol
 *   struct android_app* OPENSSL_android_native_app
 *
 * Copyright 1995-2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 *
 * On Android files like cert.pem cannot be accessed with fopen() or
 * openssl_fopen(). Android keeps the files in an APK file (just a fancy
 * ZIP file) and when you want the file, it hides the fact you are actually
 * unzipping the file while reading it.
 *
 * This pretends to be "FILE pointer" access. But it actually uses
 * AAssetManager_open() to unzip from the APK file.
 */

#include <stdio.h>
#include <errno.h>
#include <crypto/bio/bio_local.h>
#include <openssl/err.h>

#if !defined(OPENSSL_NO_STDIO)

#include <android_native_app_glue.h>
#include <android/log.h>

static int file_write(BIO *h, const char *buf, int num);
static int file_read(BIO *h, char *buf, int size);
static int file_puts(BIO *h, const char *str);
static int file_gets(BIO *h, char *str, int size);
static long file_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int file_new(BIO *h);
static int file_free(BIO *data);
static const BIO_METHOD methods_filep = {
    BIO_TYPE_FILE,
    "FILE pointer",
    /* TODO: Convert to new style write function */
    bwrite_conv,
    file_write,
    /* TODO: Convert to new style read function */
    bread_conv,
    file_read,
    file_puts,
    file_gets,
    file_ctrl,
    file_new,
    file_free,
    NULL,                      /* file_callback_ctrl */
};

// Application must set this before calling openssl functions.
struct android_app* OPENSSL_android_native_app = NULL;

static AAsset* BIO_android_asset_open(const char *filename, const char *mode)
{
    if (!OPENSSL_android_native_app) {
        __android_log_print(ANDROID_LOG_ERROR, "volcano",
                            "BIO_new_file(%s): you must set %s first",
                            filename, "OPENSSL_android_native_app");
        return NULL;
    }
    if (strncmp(filename, "assets://", 9)) {
        __android_log_print(ANDROID_LOG_ERROR, "volcano",
                            "BIO_new_file(%s) bad prefix", filename);
        return NULL;
    }
    if (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL ||
        strchr(mode, '+') != NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "volcano",
                            "BIO_new_file(%s, \"%s\") for read only Android",
                            filename, mode);
        return NULL;
    }
    AAsset* ret = AAssetManager_open(
        OPENSSL_android_native_app->activity->assetManager,
        filename + strlen("assets://") /*strip off prefix*/,
        AASSET_MODE_RANDOM);
    if (ret == NULL) {
        errno = ENOENT;
        SYSerr(SYS_F_FOPEN, get_last_sys_error());
        ERR_add_error_data(5, "AAssetManager_open('", filename, "','", mode,
                           "')");
        __android_log_print(ANDROID_LOG_ERROR, "volcano",
                            "AAssetManager_open(%s): not found", filename);
    }
    return ret;
}

BIO *BIO_new_file(const char *filename, const char *mode)
{
    BIO  *ret;
    AAsset* asset = BIO_android_asset_open(filename, mode);
    int fp_flags = BIO_CLOSE;

    if (strchr(mode, 'b') == NULL)
        fp_flags |= BIO_FP_TEXT;

    if (asset == NULL) {
        BIOerr(BIO_F_BIO_NEW_FILE, BIO_R_NO_SUCH_FILE);
        return NULL;
    }
    if ((ret = BIO_new(BIO_s_file())) == NULL) {
        AAsset_close(asset);
        return NULL;
    }

    BIO_clear_flags(ret, BIO_FLAGS_UPLINK); /* we did fopen -> we disengage
                                             * UPLINK */
    BIO_set_fp(ret, asset, fp_flags);
    return ret;
}

BIO *BIO_new_fp(FILE *stream, int close_flag)
{
  // BIO_new_file() checks the mode.
  __android_log_print(ANDROID_LOG_ERROR, "volcano",
                      "BIO_new_fp() not supported on Android");
  return NULL;
}

const BIO_METHOD *BIO_s_file(void)
{
    return &methods_filep;
}

static int file_new(BIO *bi)
{
    bi->init = 0;
    bi->num = 0;
    bi->ptr = NULL;
    bi->flags = BIO_FLAGS_UPLINK; /* default to UPLINK */
    return 1;
}

static int file_free(BIO *a)
{
    if (a == NULL)
        return 0;
    if (a->shutdown) {
        if ((a->init) && (a->ptr != NULL)) {
            AAsset_close(a->ptr);
            a->ptr = NULL;
            a->flags = BIO_FLAGS_UPLINK;
        }
        a->init = 0;
    }
    return 1;
}

static int file_read(BIO *b, char *out, int outl)
{
    int ret = 0;
    if (b->init && (out != NULL)) {
        ret = AAsset_read((AAsset*)b->ptr, out, (size_t)outl);
    }
    return ret;
}

static int file_write(BIO *b, const char *in, int inl)
{
    if (1) {
        __android_log_print(ANDROID_LOG_ERROR, "volcano",
                            "file_write() not supported on Android");
        return -1;
    }
    int ret = 0;

    if (b->init && (in != NULL)) {
        if (b->flags & BIO_FLAGS_UPLINK)
            ret = UP_fwrite(in, (int)inl, 1, b->ptr);
        else
            ret = fwrite(in, (int)inl, 1, (FILE *)b->ptr);
        if (ret)
            ret = inl;
        /* ret=fwrite(in,1,(int)inl,(FILE *)b->ptr); */
        /*
         * according to Tim Hudson <tjh@openssl.org>, the commented out
         * version above can cause 'inl' write calls under some stupid stdio
         * implementations (VMS)
         */
    }
    return ret;
}

static long file_ctrl(BIO *b, int cmd, long num, void *ptr)
{
    long ret = 1;
    FILE *fp = (FILE *)b->ptr;
    FILE **fpp;
    AAsset *asset = NULL;
    char p[4];
    int st;

    switch (cmd) {
    case BIO_C_FILE_SEEK:
    case BIO_CTRL_RESET:
        if (b->flags & BIO_FLAGS_UPLINK)
            ret = (long)UP_fseek(b->ptr, num, 0);
        else
            ret = (long)fseek(fp, num, 0);
        break;
    case BIO_CTRL_EOF:
        if (b->flags & BIO_FLAGS_UPLINK)
            ret = (long)UP_feof(fp);
        else
            ret = (long)feof(fp);
        break;
    case BIO_C_FILE_TELL:
    case BIO_CTRL_INFO:
        ret = AAsset_getLength(b->ptr) - AAsset_getRemainingLength(b->ptr);
        break;
    case BIO_C_SET_FILE_PTR:
        __android_log_print(ANDROID_LOG_ERROR, "volcano",
                            "BIO_C_SET_FILE_PTR(%p) for read only Android",
                            (char*)ptr);
        break;
    case BIO_C_SET_FILENAME:
        file_free(b);
        b->shutdown = (int)num & BIO_CLOSE;
        if (num & (BIO_FP_APPEND | BIO_FP_WRITE)) {
            __android_log_print(ANDROID_LOG_ERROR, "volcano",
                                "BIO_C_SET_FILENAME(%s) for read only Android",
                                (char*)ptr);
            ret = 0;
            break;
        } else if (!(num & BIO_FP_READ)) {
            BIOerr(BIO_F_FILE_CTRL, BIO_R_BAD_FOPEN_MODE);
            ret = 0;
            break;
        }
        asset = BIO_android_asset_open(ptr, p);
        if (asset == NULL) {
            BIOerr(BIO_F_FILE_CTRL, ERR_R_SYS_LIB);
            ret = 0;
            break;
        }
        b->ptr = asset;
        b->init = 1;
        BIO_clear_flags(b, BIO_FLAGS_UPLINK); /* we did fopen -> we disengage
                                               * UPLINK */
        break;
    case BIO_C_GET_FILE_PTR:
        if (1) {
            __android_log_print(ANDROID_LOG_ERROR, "volcano",
                                "BIO_get_fp() not supported on Android");
            return 1;
        }
        /* the ptr parameter is actually a FILE ** in this case. */
        if (ptr != NULL) {
            fpp = (FILE **)ptr;
            *fpp = (FILE *)b->ptr;
        }
        break;
    case BIO_CTRL_GET_CLOSE:
        ret = (long)b->shutdown;
        break;
    case BIO_CTRL_SET_CLOSE:
        b->shutdown = (int)num;
        break;
    case BIO_CTRL_FLUSH:
        if (0) {
            /* Android only supports read-only so flush is a no-op */
            st = b->flags & BIO_FLAGS_UPLINK
                    ? UP_fflush(b->ptr) : fflush((FILE *)b->ptr);
            if (st == EOF) {
                SYSerr(SYS_F_FFLUSH, get_last_sys_error());
                ERR_add_error_data(1, "fflush()");
                BIOerr(BIO_F_FILE_CTRL, ERR_R_SYS_LIB);
                ret = 0;
            }
        }
        break;
    case BIO_CTRL_DUP:
        ret = 1;
        break;

    case BIO_CTRL_WPENDING:
    case BIO_CTRL_PENDING:
    case BIO_CTRL_PUSH:
    case BIO_CTRL_POP:
    default:
        ret = 0;
        break;
    }
    return ret;
}

static int file_gets(BIO *bp, char *buf, int size)
{
    int ret = 0;

    buf[0] = '\0';
    if (bp->init && (buf != NULL)) {
        ret = AAsset_read((AAsset*)bp->ptr, buf, (size_t)size - 1);
        if (ret > 0) {
          /* Convert buf into a valid string by adding a null terminator */
          buf[ret] = '\0';
          /* Locate the newline, if any */
          size_t pos = strcspn(buf, "\r\n");
          pos = pos + strspn(buf + pos, "\r\n");
          if (pos < ret) {
              /* Shorten buf, then seek backward to re-read those bytes. */
              buf[pos] = '\0';
              off_t seekresult = AAsset_seek((AAsset*)bp->ptr,
                                             (int)pos - (int)ret, SEEK_CUR);
              if (seekresult < 0) {
                  __android_log_print(ANDROID_LOG_ERROR, "volcano",
                                      "file_gets: seek(%d) failed",
                                      (int)pos - (int)ret);
                  ret = 0;
                  goto err;
              }
              ret = pos;
          }
        }
    }
 err:
    return ret;
}

static int file_puts(BIO *bp, const char *str)
{
    if (1) {
        __android_log_print(ANDROID_LOG_ERROR, "volcano",
                            "file_puts() not supported on Android");
        return -1;
    }
    int n, ret;

    n = strlen(str);
    ret = file_write(bp, str, n);
    return ret;
}

#else

static int file_write(BIO *b, const char *in, int inl)
{
    return -1;
}
static int file_read(BIO *b, char *out, int outl)
{
    return -1;
}
static int file_puts(BIO *bp, const char *str)
{
    return -1;
}
static int file_gets(BIO *bp, char *buf, int size)
{
    return 0;
}
static long file_ctrl(BIO *b, int cmd, long num, void *ptr)
{
    return 0;
}
static int file_new(BIO *bi)
{
    return 0;
}
static int file_free(BIO *a)
{
    return 0;
}

static const BIO_METHOD methods_filep = {
    BIO_TYPE_FILE,
    "FILE pointer",
    /* TODO: Convert to new style write function */
    bwrite_conv,
    file_write,
    /* TODO: Convert to new style read function */
    bread_conv,
    file_read,
    file_puts,
    file_gets,
    file_ctrl,
    file_new,
    file_free,
    NULL,                      /* file_callback_ctrl */
};

const BIO_METHOD *BIO_s_file(void)
{
    return &methods_filep;
}

BIO *BIO_new_file(const char *filename, const char *mode)
{
    return NULL;
}

#endif                         /* OPENSSL_NO_STDIO */
