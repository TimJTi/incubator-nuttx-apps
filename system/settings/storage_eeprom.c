/****************************************************************************
 * apps/system/settings/storage_eeprom.c
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

#include "system/settings.h"
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <nuttx/crc32.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <nuttx/config.h>
#include <sys/types.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VALID          0x600d
#define BUFFER_SIZE    (sizeof(setting_t)) /* just one setting */

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

FAR static setting_t *getsetting(char *key);
extern setting_t map[CONFIG_SYSTEM_SETTINGS_MAP_SIZE];

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: load_eeprom
 *
 * Description:
 *    Loads binary data from an EEPROM storage file.
 *
 * Input Parameters:
 *    file             - the filename of the storage to use
 *
 * Returned Value:
 *   Success or negated failure code
 *
 ****************************************************************************/

int load_eeprom(FAR char *file)
{
  int           fd;
  int           i;
  int           ret = OK;
  uint16_t      valid;
  uint16_t      count;
  uint32_t      calc_crc = 0;
  uint32_t      exp_crc = 0;
  setting_t     setting;
  FAR setting_t *slot;

  fd = open(file, O_RDONLY);
  if (fd < 0)
    {
      return -ENOENT;
    }

  valid = 0;
  read(fd, &valid, sizeof(uint16_t));

  if (valid != VALID)
    {
      ret = -EBADMSG;
      goto abort; /* Just exit - the settings aren't valid */
    }

  count = 0;
  read(fd, &count, sizeof(uint16_t));

  for (i = 0; i < count; i++)
    {
      read(fd, &setting, sizeof(setting_t));
      calc_crc = crc32part((FAR uint8_t *)&setting, sizeof(setting_t),
                            calc_crc);
    }

  if (calc_crc != exp_crc)
    {
      ret = -EBADMSG;
      goto abort;
    }

  lseek(fd, (sizeof(uint16_t) * 2), SEEK_SET);  /* Get after valid & size */

  for (i = 0; i < count; i++)
    {
      read(fd, &setting, sizeof(setting_t));

      slot = getsetting(setting.key);
      if (slot == NULL)
        {
          continue;
        }

      memcpy(slot, &setting, sizeof(setting_t));
    }

abort:
  close(fd);
  return ret;
}

/****************************************************************************
 * Name: save_eeprom
 *
 * Description:
 *    Saves binary data to an EEPROM storage file.
 *
 * Input Parameters:
 *    file             - the filename of the storage to use
 *
 * Returned Value:
 *   Success or negated failure code
 *
 ****************************************************************************/

int save_eeprom(FAR char *file)
{
  uint16_t      valid;
  uint16_t      count;
  uint16_t      eeprom_cnt;
  int           fd;
  int           ret = OK;
  int           i;
  uint32_t      new_crc   = 0;
  uint32_t      old_crc   = 0;
  setting_t     old_setting;
  FAR setting_t new_setting;
  off_t         offset;
  FAR uint8_t   *buffer = malloc(BUFFER_SIZE);

  if (buffer == NULL)
    {
      return -ENOMEM;
    }

  /* How many settings do we have? */

  count = 0;
  for (i = 0; i < CONFIG_SYSTEM_SETTINGS_MAP_SIZE; i++)
    {
      if (map[i].type == SETTING_EMPTY)
        {
          break;
        }

      count++;
    }

  fd = open(file, (O_RDWR | O_TRUNC), 0666);
  if (fd < 0)
    {
      ret = -ENODEV;
      goto abort;
    }

  if (count == 0)
    {
      /* nothing stored yet */

      valid = VALID;
      write(fd, &valid, sizeof(valid));
      goto abort_withfd;
    }

  read(fd, &valid, sizeof(valid));

  if (valid != VALID)
    {
      ret = -EBADMSG;
      goto abort_withfd; /* Just exit - the settings aren't valid */
    }

  read(fd, &eeprom_cnt, sizeof(uint16_t));
  if (count != eeprom_cnt)
    {
      offset = sizeof(valid);
      lseek(fd, offset, SEEK_SET);
      write(fd, &count, sizeof(count));
    }

  offset = sizeof(valid) + sizeof(eeprom_cnt);
  lseek(fd, offset, SEEK_SET); /* Make sure we're at start of settings */

  for (i = 0; i < count; i++)
    {
      offset = (sizeof(uint16_t) * 2) + (i * sizeof(setting_t));
      lseek(fd, offset, SEEK_SET);
      read(fd, &old_setting, sizeof(setting_t));
      new_setting = map[i];
      new_crc = crc32((FAR uint8_t *)&new_setting, sizeof(setting_t));
      old_crc = crc32((FAR uint8_t *)&old_setting, sizeof(setting_t));
      if (new_crc == old_crc)
        {
          continue;
        }

      /* Write the changed value */

      lseek(fd, offset, SEEK_SET);
      write(fd, &new_setting, sizeof(setting_t));
    }

  /* append crc */

  new_crc = crc32((FAR uint8_t *)map, (count * sizeof(setting_t)));
  write(fd, &new_crc, sizeof(new_crc));

abort_withfd:
  close(fd);
abort:
  free(buffer);

  return ret;
}

/****************************************************************************
 * Name: getsetting
 *
 * Description:
 *    Gets the setting information from a given key.
 *
 * Input Parameters:
 *    key        - key of the required setting
 *
 * Returned Value:
 *   The setting
 *
 ****************************************************************************/

FAR setting_t *getsetting(FAR char *key)
{
  int i;

  for (i = 0; i < CONFIG_SYSTEM_SETTINGS_MAP_SIZE; i++)
    {
      FAR setting_t *setting = &map[i];

      if (strcmp(key, setting->key) == 0)
        {
          return setting;
        }

      if (setting->type == SETTING_EMPTY)
        {
          return setting;
        }
    }

  return NULL;
}
