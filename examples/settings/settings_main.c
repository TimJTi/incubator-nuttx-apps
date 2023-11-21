/****************************************************************************
 * apps/examples/settings/settings_main.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/param.h>

#include "system/settings.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * settings_main
 ****************************************************************************/

int settings_main(int argc, FAR char *argv[])
{
  int                  ret;
  int                  fd;
  enum settings_type_e stype;
  char                 path[CONFIG_PATH_MAX];
  const char           teststr[] = "I'm a string";
  int                  testval = 0x5aa5;
  char                 readstr[CONFIG_SYSTEM_SETTINGS_VALUE_SIZE];
  enum storage_type_e  storage_type = STORAGE_BINARY;
  bool                 flag_present = false;
  struct stat          sbuf;

  if ((argc < 1) || *argv[1] == 0 || *(argv[1] + 1) == 0)
    {
      goto print_help;
    }

  if (*argv[1] == '-')
    {
      flag_present = true;
      if (*(argv[1] + 1) == 'b')
        {
          storage_type = STORAGE_BINARY;
        }
      else if (*(argv[1] + 1) == 't')
        {
          storage_type = STORAGE_TEXT;
        }
      else
        {
          goto print_help;
        }
    }

  if (flag_present && argc < 2)
    {
      goto print_help;
    }

#ifdef CONFIG_EXAMPLES_SETTINGS_USE_TMPFS
#  ifndef CONFIG_FS_TMPFS
#    error TMPFS must be enabled
#  endif
#  ifndef CONFIG_LIBC_TMPDIR
#    error LIBC_TMPDIR must be defined
#  endif
  if (stat(CONFIG_LIBC_TMPDIR, &sbuf))
    {
      ret = nx_mount(NULL, CONFIG_LIBC_TMPDIR, "tmpfs", 0, NULL);
      if (ret < 0)
        {
          printf("ERROR: Failed to mount tmpfs at %s: %d\n",
                  CONFIG_LIBC_TMPDIR, ret);
          goto end;
        }
    }

  strcpy(path, CONFIG_LIBC_TMPDIR);
#else
  strcpy(path, CONFIG_EXAMPLES_SETTINGS_EXISTING_STORAGE);
  if (path == NULL)
    {
      printf("Settings filepath is empty!");
      goto end;
    }
#endif

  strcat(path, "/");
  strcat(path, CONFIG_EXAMPLES_SETTINGS_FILENAME);

  printf("Example of settings usage: %s. Path: %s\n",
         ((storage_type == STORAGE_TEXT) ? "text" : "binary"), path);
  printf("--------------------------------------------------------------\n");

  ret = settings_init();
  if (ret < 0)
    {
      printf("settings init failed: %d", ret);
      goto end;
    }

  ret = settings_setstorage(path, storage_type);
  if (ret == -ENOENT)
    {
      printf("No existing storage file found. Creating it.\n");
      fd = open(path, O_CREAT);
      if (fd < 0)
        {
          printf("Failed to create settings file\n");
          goto end;
        }

      close(fd);
    }
  else if (ret < 0)
    {
      printf("settings setstorage failed: %d\n", ret);
      goto end;
    }
  else
    {
      printf("existing settings storage file foung\n");
    }

  ret = settings_create("v1", SETTING_STRING, "default value");
  if (ret < 0)
    {
      printf("settings create failed: %d\n", ret);
      goto end;
    }

  /* if this app has been run before, the setting type is likely changed */

  ret = settings_type("v1", &stype);
  if (ret < 0)
    {
      printf("Failed to get settings type: %d\n", ret);
      goto end;
    }

  if (stype == SETTING_STRING)
    {
      ret = settings_get("v1", SETTING_STRING, &readstr, sizeof(readstr));
      if (ret < 0)
        {
          printf("settings retrieve failed: %d\n", ret);
          goto end;
        }

      printf("Retrieved settings value (v1) with value:%s\n", readstr);
    }
  else
    {
      ret = settings_get("v1", SETTING_INT, &testval,
                          CONFIG_SYSTEM_SETTINGS_VALUE_SIZE);
      if (ret < 0)
        {
          printf("settings retrieve failed: %d\n", ret);
          goto end;
        }

      printf("Retrieved settings value (v1) with value:%d\n", testval);
    }

  printf("Trying to (re)create a setting that already exists (v1)\n");

  testval = 0xa5a5;
  ret = settings_create("v1", SETTING_INT, testval);
  if (ret == -EACCES)
    {
      printf("Deliberate fail: setting exists! Error: %d\n", ret);
    }
  else if (ret < 0)
    {
      printf("settings create failed: %d\n", ret);
      goto end;
    }

  ret = settings_type("v1", &stype);
  if (ret < 0)
    {
      printf("failed to read settings type: %d\n", ret);
      goto end;
    }

  printf("Retrieved setting type is: %d\n", stype);

  printf("Trying to change setting (v1) to integer type\n");
  ret = settings_set("v1", SETTING_INT, &testval);
  if (ret < 0)
    {
      printf("Deliberate fail: settings change invalid: %d\n", ret);
    }

  ret = settings_get("v2", SETTING_INT, &testval);
  if (ret < 0)
    {
      printf("Deliberate fail: non-existent setting requested. Error:%d\n",
             ret);
    }

  printf("Trying to change setting (v1) from int to string: %s\n", teststr);
  ret = settings_set("v1", SETTING_STRING, &teststr);
  if (ret < 0)
    {
      printf("Deliberate fail: settings change invalid: %d\n", ret);
    }

  printf("Creating a string settings value (s1):%s\n", teststr);
  ret = settings_create("s1", SETTING_STRING, teststr);
  if (ret < 0)
    {
      printf("settings changed failed: %d\n", ret);
      goto end;
    }

  ret = settings_get("s1", SETTING_STRING, &readstr, sizeof(readstr));
  if (ret < 0)
    {
      printf("settings retrieve failed: %d\n", ret);
      goto end;
    }

  printf("Retrieved string settings value (s1) with value:%s\n",
          readstr);

  FAR struct in_addr save_ip;
  FAR struct in_addr load_ip;
  save_ip.s_addr = HTONL(0xc0a86401);

  printf("Changing setting to an IP value (s1) with value:0x%08lx\n",
          save_ip.s_addr);
  ret = settings_set("s1", SETTING_IP_ADDR, &save_ip);
  if (ret < 0)
    {
      printf("IP address settings create failed: %d\n", ret);
      goto end;
    }

  ret = settings_get("s1", SETTING_IP_ADDR, &load_ip);
  if (ret < 0)
    {
      printf("IP address settings retrieve failed: %d\n", ret);
      goto end;
    }

  printf("Retrieved IP address settings value (s1) with value:0x%08lx\n",
          NTOHL(load_ip.s_addr));
end:

#ifdef CONFIG_SYSTEM_SETTINGS_CACHED_SAVES
  /* Cached saves may not have been written out yet */

  usleep(2 * 1000 * CONFIG_SYSTEM_SETTINGS_CACHE_TIME_MS);
#endif
  return ret;

print_help:
  printf("Usage...\n");
  printf("settings [-b | -t] \n");
  printf("    -i = use a binary storage file (default)\n");
  printf("    -t = use a text   storage file\n");
  printf(" Example:\n");
  printf("   settings -b\n");
  return -EINVAL;
}
