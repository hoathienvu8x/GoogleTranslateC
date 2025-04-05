// SPDX-License-Identifier: GPL-2.0
/*
 *  main.c
 *
 *  Google Translate scraper library usage example.
 *
 *  Copyright (C) 2021  Ammar Faizi
 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sqlite3.h>

#include "cgtranslate.h"
#include "sha1.h"

static char * my_strdup(const char * s)
{
  size_t len = 1+strlen(s);
  char *p = malloc(len);

  return p ? memcpy(p, s, len) : (errno = ENOMEM, NULL);
}
/* https://gist.github.com/JonathonReinhart/8c0d90191c38af2dcadb102c4e202950 */
/* Make a directory; already existing dir okay */
static int maybe_mkdir(const char* path, mode_t mode)
{
  struct stat st;
  errno = 0;

  /* Try to make the directory */
  if (mkdir(path, mode) == 0)
    return 0;

  /* If it fails for any reason but EEXIST, fail */
  if (errno != EEXIST)
    return -1;

  /* Check if the existing path is a directory */
  if (stat(path, &st) != 0)
    return -1;

  /* If not, fail with ENOTDIR */
  if (!S_ISDIR(st.st_mode)) {
    errno = ENOTDIR;
    return -1;
  }

  errno = 0;
  return 0;
}

int mkdir_p(const char *path)
{
  /* Adapted from http://stackoverflow.com/a/2336245/119527 */
  char *_path = NULL;
  char *p; 
  int result = -1;
  mode_t mode = 0777;

  errno = 0;

  /* Copy string so it's mutable */
  _path = my_strdup(path);
  if (_path == NULL)
    goto out;

  /* Iterate the string */
  for (p = _path + 1; *p; p++) {
    if (*p == '/') {
      /* Temporarily truncate */
      *p = '\0';

      if (maybe_mkdir(_path, mode) != 0)
        goto out;

      *p = '/';
    }
  }   

  if (maybe_mkdir(_path, mode) != 0)
    goto out;

  result = 0;

out:
  free(_path);
  return result;
}

int cgtr_get_local_trans(sqlite3 *db, const char *hash_code,
                    const char *dst_str, char **out) {
  int idx = 0, ret = -1;
  sqlite3_stmt *stmt;
  const char *sql = "select translate_text from gtranslates where "
        "translate_hash = @hash_code and translate_dst = @dst_str;";
  if (sqlite3_prepare(db, sql, -1, &stmt, 0) != SQLITE_OK) {
    printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  idx = sqlite3_bind_parameter_index(stmt, "@hash_code");
  if (idx <= 0) {
    sqlite3_finalize(stmt);
    return -1;
  }
  if (sqlite3_bind_text(stmt, idx, hash_code, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
    printf("Failed to bind @hash_code: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return -1;
  }

  idx = sqlite3_bind_parameter_index(stmt, "@dst_str");
  if (idx <= 0) {
    sqlite3_finalize(stmt);
    return -1;
  }
  if (sqlite3_bind_text(stmt, idx, dst_str, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
    printf("Failed to bind @dst_str: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return -1;
  }

  ret = sqlite3_step(stmt);
  if (ret == SQLITE_ROW) {
    *out = my_strdup((const char *)sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    return 0;
  }
  sqlite3_finalize(stmt);
  return -1;
}

int cgtr_init_local_trans(sqlite3 *db) {
  char *err_msg = 0;
  const char *sql = "CREATE TABLE IF NOT EXISTS gtranslates (translate_hash"
                        " TEXT, translate_dst TEXT, translate_text TEXT);";
  if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
    printf("SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return -1;
  }
  return 0;
}

int cgtr_set_local_trans(sqlite3 *db, const char *hash_code,
                const char *dst_str, const char *text) {
  sqlite3_stmt *stmt;
  int idx = 0, ret = -1;
  const char *sql = "insert into gtranslates (translate_hash, translate_dst,"
            " translate_text) values (@hash_code, @dst_str, @text_trans);";
  char *text_trans = sqlite3_mprintf("%q", text);

  if (!text_trans) {
    printf("Failed to create text_trans: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  if (sqlite3_prepare(db, sql, -1, &stmt, 0) != SQLITE_OK) {
    printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
    sqlite3_free(text_trans);
    return -1;
  }

  idx = sqlite3_bind_parameter_index(stmt, "@hash_code");
  if (idx <= 0) {
    sqlite3_free(text_trans);
    sqlite3_finalize(stmt);
    return -1;
  }
  if (sqlite3_bind_text(stmt, idx, hash_code, -1, SQLITE_STATIC) != SQLITE_OK) {
    printf("Failed to bind @hash_code: %s\n", sqlite3_errmsg(db));
    sqlite3_free(text_trans);
    sqlite3_finalize(stmt);
    return -1;
  }

  idx = sqlite3_bind_parameter_index(stmt, "@dst_str");
  if (idx <= 0) {
    sqlite3_free(text_trans);
    sqlite3_finalize(stmt);
    return -1;
  }
  if (sqlite3_bind_text(stmt, idx, dst_str, -1, SQLITE_STATIC) != SQLITE_OK) {
    printf("Failed to bind @dst_str: %s\n", sqlite3_errmsg(db));
    sqlite3_free(text_trans);
    sqlite3_finalize(stmt);
    return -1;
  }

  idx = sqlite3_bind_parameter_index(stmt, "@text_trans");
  if (idx <= 0) {
    sqlite3_free(text_trans);
    sqlite3_finalize(stmt);
    return -1;
  }
  if (sqlite3_bind_text(stmt, idx, text_trans, -1, SQLITE_STATIC) != SQLITE_OK) {
    printf("Failed to bind @text_trans: %s\n", sqlite3_errmsg(db));
    sqlite3_free(text_trans);
    sqlite3_finalize(stmt);
    return -1;
  }

  ret = sqlite3_step(stmt);
  if (ret == SQLITE_DONE) {
    printf("Save translate ok\n");
    sqlite3_finalize(stmt);
    return 0;
  }
  sqlite3_finalize(stmt);
  return -1;
}

int main(int argc, const char **argv)
{
  int ret;
  char curdir[128];
  char cache_dir[256];
  char cookie_dir[256];
  char sqlitedb_dir[256];
  char sha1_hash[SHA1_BLOCK_SIZE * 2 + 1] = {0};
  char *result = NULL;
  const char *from = NULL;
  const char *to = NULL;
  const char *text = NULL;
  cgtranslate_t *cg;
  sqlite3 *db;

  if (argc < 2) {
    printf("Usage: %s <text>\n", argv[0]);
    return -1;
  }

  from = getenv("CGTRANSLATE_FROM");
  to = getenv("CGTRANSLATE_TO");
  text = argv[1];

  if (
    !from || strlen(from) == 0 ||
    !to || strlen(to) == 0 ||
    strlen(text) == 0
  ) {
    return -1;
  }

  /*
   * Get current working directory (for cookie and cache)
   */
  if (!getcwd(curdir, sizeof(curdir))) {
    ret = errno;
    printf("Error: getcwd(): %s\n", strerror(ret));
    return ret;
  }

  SHA1_Hash((const uint8_t *)text, strlen(text), sha1_hash);
  sha1_hash[7] = '\0';

  snprintf(cache_dir, sizeof(cache_dir), "%s/data/cache", curdir);
  snprintf(cookie_dir, sizeof(cookie_dir), "%s/data/cookie", curdir);
  snprintf(sqlitedb_dir, sizeof(sqlitedb_dir), "%s/data/translates.db", curdir);

  if (mkdir_p(cache_dir) || mkdir_p(cookie_dir)) {
    ret = errno;
    printf("Error: mkdir_p(cache_dir) or mkdir_p(cookie_dir): %s\n", strerror(ret));
    return ret;
  }

  ret = sqlite3_open(sqlitedb_dir, &db);
  if (ret != SQLITE_OK) {
    printf("Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return ret;
  }

  ret = cgtr_init_local_trans(db);
  if (ret) {
    sqlite3_close(db);
    return ret;
  }

  cgtr_global_init();

  cg = cgtr_init();
  if (!cg) {
    ret = errno;
    printf("Error: cgtr_init(): %s\n", strerror(ret));
    return ret;
  }

  
  ret = cgtr_set_cache_dir(cg, cache_dir);
  if (ret != 0) {
    ret = -ret;
    printf("Error: cgtr_set_cache_dir(%s): %s\n", cache_dir,
      cgtr_get_err_str(cg));
    goto out_free;
  }


  ret = cgtr_set_cookie_dir(cg, cookie_dir);
  if (ret != 0) {
    ret = -ret;
    printf("Error: cgtr_set_cookie_dir(%s): %s\n", cookie_dir,
      cgtr_get_err_str(cg));
    goto out_free;
  }


  ret = cgtr_set_lang(cg, from, to);
  if (ret != 0) {
    ret = -ret;
    printf("Error: cgtr_set_lang(from=%s, to=%s): %s\n",
           from, to, cgtr_get_err_str(cg));
    goto out_free;
  }


  ret = cgtr_set_text_ref(cg, text);
  if (ret != 0) {
    ret = -ret;
    printf("Error: cgtr_set_text(): %s\n", cgtr_get_err_str(cg));
    goto out_free;
  }

  if (cgtr_get_local_trans(db, sha1_hash, to, &result) == 0) {
    goto out_done;
  }

  ret = cgtr_execute(cg);
  if (ret != 0) {
    ret = -ret;
    printf("Error: cgtr_execute(): %s\n", cgtr_get_err_str(cg));
    goto out_free;
  }


  result = cgtr_detach_result(cg);
  if (!result) {
    printf("Got null pointer!\n");
    goto out_free;
  }

  (void)cgtr_set_local_trans(db, sha1_hash, to, result);

out_done:
  printf("Source language: %s\n", from);
  printf("Target language: %s\n", to);
  printf("Text source: %s\n", text);
  printf("Translate result = %s\n", result);

out_free:
  if (result) free(result);
  cgtr_destroy(cg);
  cgtr_global_close();
  return ret;
}
