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
 *
 * The EEPROM storage is similar to the binary type, but only saves out
 * values if they've actually changed, to maximise device life.
 *
 * It can, of course, be used with other storage media types that have
 * limited write cycle capabilities.
 *
 * RE-VISIT IN FUTURE - This could be enhanced by allowing for variable
 * storage sizes, but gets complicated if the settings type, and hence size,
 * changes as settings after the saved storage will need to move/change.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "system/settings.h"
#include <unistd.h>
#include <errno.h>
#include <assert.h>
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

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

FAR static setting_t *getsetting(char *key);
size_t getsettingsize(enum settings_type_e type);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static size_t used_storage;

/****************************************************************************
 * Public Data
 ****************************************************************************/

extern setting_t map[CONFIG_SYSTEM_SETTINGS_MAP_SIZE];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

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

/****************************************************************************
 * Name: getsettingsize
 *
 * Description:
 *    Helper function to gets the size of the setting type for a given type.
 *
 * Input Parameters:
 *    type        - the type to get the size of
 *
 * Returned Value:
 *   The setting
 *
 ****************************************************************************/

size_t getsettingsize(enum settings_type_e type)
{
  switch (type)
    {
      case SETTING_STRING:
        {
          return CONFIG_SYSTEM_SETTINGS_VALUE_SIZE * (sizeof(char));
        }
      break;
      case SETTING_BOOL:
        {
          return sizeof(bool);
        }
      break;
      case SETTING_INT:
        {
          return sizeof(int);
        }
      break;
      case SETTING_BYTE:
        {
          return sizeof(uint8_t);
        }
      break;
      case SETTING_FLOAT:
        {
          return sizeof(float);
        }
      break;
      case SETTING_IP_ADDR:
        {
          return sizeof(struct in_addr);
        }
      break;
      case SETTING_EMPTY:
      default:
        {
          return 0;
        }
      break;
    }
}

/****************************************************************************
 * Public Functions
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
  size_t        size;

  fd = open(file, O_RDONLY);
  if (fd < 0)
    {
      return -ENOENT;
    }

  used_storage = 0;
  valid = 0;
  read(fd, &valid, sizeof(valid));

  if (valid != VALID)
    {
      ret = -EBADMSG;
      goto abort; /* Just exit - the settings aren't valid */
    }

  count = 0;
  read(fd, &count, sizeof(count));

  /* check storage integrity using crc check */

  for (i = 0; i < count; i++)
    {
      read(fd, &setting.key, CONFIG_SYSTEM_SETTINGS_KEY_SIZE);
      read(fd, &setting.type, sizeof(uint16_t));
      size = getsettingsize(setting.type);
      read(fd, &setting.val, size);
      size += (CONFIG_SYSTEM_SETTINGS_KEY_SIZE + sizeof(uint16_t));
      calc_crc = crc32part((FAR uint8_t *)&setting, size, calc_crc);
    }

  lseek(fd, 0, SEEK_CUR);
  read(fd, &exp_crc, sizeof(exp_crc));
  if (calc_crc != exp_crc)
    {
      ret = -EBADMSG;
      goto abort;
    }

  lseek(fd, (sizeof(count) + sizeof(valid)), SEEK_SET);

  used_storage = sizeof(valid) + sizeof(count) + sizeof(calc_crc);

  for (i = 0; i < count; i++)
    {
      read(fd, &setting.key, CONFIG_SYSTEM_SETTINGS_KEY_SIZE);
      read(fd, &setting.type, sizeof(uint16_t));
      size = getsettingsize(setting.type);
      read(fd, &setting.val, size);
      slot = getsetting(setting.key);
      if (slot == NULL)
        {
          continue;
        }

      size += (CONFIG_SYSTEM_SETTINGS_KEY_SIZE + sizeof(uint16_t));
      used_storage += size;
      memcpy(slot, &setting, size);
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
 *    The first two stored values are:
 *      1) "valid" - set to a "magic" number when store is valid
 *      2) "count" - the number of settings in the store
 *    At the end of the store a crc is saved of all settings data (excluding
 *    "valid and "count")
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
  uint16_t      valid = 0;
  uint16_t      count = 0;
  uint16_t      eeprom_cnt;
  int           fd;
  int           ret = OK;
  int           i;
  uint32_t      crc = 0;
  setting_t     old_setting;
  setting_t     new_setting;
  size_t        new_size;
  size_t        old_size;

  /* How many settings do we have? */

  for (i = 0; i < CONFIG_SYSTEM_SETTINGS_MAP_SIZE; i++)
    {
      if (map[i].type == SETTING_EMPTY)
        {
          break;
        }

      count++;
    }

  used_storage = 0;
  fd = open(file, (O_RDWR | O_TRUNC), 0666);
  if (fd < 0)
    {
      ret = -ENODEV;
      return ret;
    }

  read(fd, &valid, sizeof(valid));
  eeprom_cnt = 0;
  read(fd, &eeprom_cnt, sizeof(eeprom_cnt));

  for (i = 0; i < count; i++)
    {
      new_setting = map[i];
      new_size = getsettingsize(new_setting.type);
      new_size += CONFIG_SYSTEM_SETTINGS_KEY_SIZE + sizeof(uint16_t);
      read(fd, &old_setting.key, CONFIG_SYSTEM_SETTINGS_KEY_SIZE);
      read(fd, &old_setting.type, sizeof(uint16_t));
      old_size = getsettingsize(old_setting.type);
      read(fd, &old_setting.val, old_size);
      old_size += (CONFIG_SYSTEM_SETTINGS_KEY_SIZE + sizeof(uint16_t));
      if ((i < eeprom_cnt) && (new_size > old_size))
        {
          /* We can only change /existing/ type if the size is no larger.
           * As this can be called via a timer callback, we can't easily
           * return an error code.
           */

          DEBUGASSERT(0);
        }

      if (crc32((FAR uint8_t *)&new_setting, sizeof(new_setting)) !=
          crc32((FAR uint8_t *)&old_setting, sizeof(old_setting)))
        {
          /* Only write the value if changed, or setting was EMPTY */

          lseek(fd, -old_size, SEEK_CUR); /* rewind */
          write(fd, &new_setting, new_size);
          crc = crc32part((FAR uint8_t *)&new_setting, new_size, crc);
          lseek(fd, -new_size, SEEK_CUR); /* rewind */
          read(fd, &old_setting, SEEK_CUR);
          if (crc32((FAR uint8_t *)&new_setting, sizeof(new_setting)) !=
          crc32((FAR uint8_t *)&old_setting, sizeof(new_setting)))
            {
              return -EIO;
            }
          used_storage += new_size;

#warning NB: still need to check the verify addition.

        }
      else
        {
          lseek(fd, (old_size - new_size), SEEK_CUR); /* FWD if needed */
          crc = crc32part((FAR uint8_t *)&old_setting, old_size, crc);
          used_storage += old_size;
        }
    }

  write(fd, &crc, sizeof(crc));

  lseek(fd, 0, SEEK_SET);
  if (valid != VALID) /* Only write if changed */
    {
      valid = VALID;
      write(fd, &valid, sizeof(valid));
    }

  lseek(fd, sizeof(valid), SEEK_SET);

  if (eeprom_cnt != count)  /* Only write if changed */
    {
      write(fd, &count, sizeof(count));
    }

  used_storage += sizeof(valid) + sizeof(eeprom_cnt) + sizeof(crc);

  close(fd);

  return ret;
}

/****************************************************************************
 * Name: size_eeprom
 *
 * Description:
 *    Returns the total storage size used (in bytes).
 *
 * Input Parameters:
 *    used      - pointer to struct to return used size of a given storage
 *
 * Returned Value:
 *    Success or negated failure code
 *
 ****************************************************************************/

int size_eeprom(storage_used_t *used)
{
  DEBUGASSERT(used != NULL);
  used->size = used_storage;
  return OK;
}
