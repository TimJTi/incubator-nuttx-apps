/****************************************************************************
 * apps/include/system/settings.h
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

#ifndef UTILS_SETTINGS_H_
#define UTILS_SETTINGS_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VALID          0x600d /* "Magic" number to confirm storage is OK */

/****************************************************************************
 * Public Types
 ****************************************************************************/

enum settings_type_e
{
  SETTING_EMPTY   = 0,
  SETTING_INT,        /* INT32 */
  SETTING_BOOL,
  SETTING_FLOAT,
  SETTING_STRING,
  SETTING_IP_ADDR,
  SETTING_BYTE,
};

typedef struct
{
  int    store_num;
  size_t size;
} storage_used_t;

typedef struct
{
  char key[CONFIG_SYSTEM_SETTINGS_KEY_SIZE];
  enum settings_type_e type;
  union
  {
    int    i;
    double f;
    char   s[CONFIG_SYSTEM_SETTINGS_VALUE_SIZE];
    struct in_addr ip;
  } val;
} setting_t;

typedef struct
{
  char file[CONFIG_SYSTEM_SETTINGS_MAX_FILENAME];

  int (*load_fn)(FAR char *file);
  int (*save_fn)(FAR char *file);
  int (*size_fn)(FAR storage_used_t *used);
} storage_t;

struct notify_s
{
  pid_t pid;
  uint8_t signo;
};

enum storage_type_e
{
  STORAGE_BINARY = 0,
  STORAGE_TEXT,
  STORAGE_EEPROM,
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: settings_savepending
 *
 * Description:
 *    Check if a save is pending. Useful if cached saves are in play.
 *
 * Input Parameters:
 *    none
 *
 * Returned Value:
 *    whether a save is pending: true or false;
 *
 ****************************************************************************/

bool settings_savepending(void);

/****************************************************************************
 * Name: settings_init
 *
 * Description:
 *   Initializes the settings storage.
 *
 * Input Parameters:  none
 *
 * Returned Value:    Success
 *
 ****************************************************************************/

int settings_init(void);

/****************************************************************************
 * Name: settings_setstorage
 *
 * Description:
 *    Sets a file to be used as a settings storage.
 *    Except from the first file, if loading the file causes any changes
 *    to the settings, then the new map will be dumped to all files
 *    (effectively it syncs all storages).
 *
 * Input Parameters:
 *    file             - the filename of the storage to use
 *    type             - the type of the storage (BINARY or TEXT)
 *
 * Returned Value:
 *   Success or negated failure code
 *
 ****************************************************************************/

int settings_setstorage(FAR char *file, enum storage_type_e type);

/****************************************************************************
 * Name: settings_sync
 *
 * Description:
 *    Synchronizes the storage.
 *
 * Input Parameters:
 *    none
 *
 * Returned Value:
 *    Success or negated failure code
 *
 ****************************************************************************/

int settings_sync(void);

/****************************************************************************
 * Name: settings_notify
 *
 * Description:
 *    Registers a task to be notified on any change of the settings.
 *    Whenever any value is changed, a signal will be sent to all
 *    registered threads. Signals are NOT sent when new settings are
 *    created or when the whole storage is cleared.
 *
 * Input Parameters:
 *    none
 *
 * Returned Value:
 *    Success or negated failure code
 *
 ****************************************************************************/

int settings_notify(void);

/****************************************************************************
 * Name: settings_hash
 *
 * Description:
 *    Gets the hash of the settings storage.
 *
 *    This hash represents the internal state of the settings map. A
 *    unique number is calculated based on the contents of the whole map.
 *
 *    This hash can be used to check the settings for any alterations, i.e.
 *    any setting that may had its value changed since last check.
 *
 * Input Parameters:
 *    none
 *
 * Returned Value:
 *    Success or negated failure code
 *
 ****************************************************************************/

int settings_hash(FAR uint32_t *h);

/****************************************************************************
 * Name: settings_clear
 *
 * Description:
 *    Clears all settings.
 *    Data in all storages are purged.
 *
 *    Note that if the settings are cleared during the application run-time
 *    (i.e. not during initialization), every access to the settings storage
 *    will fail.
 *
 *    All settings must be created again. This can be done either by calling
 *    Settings_create() again, or by restarting the application.
 *
 * Input Parameters:
 *    none
 *
 * Returned Value:
 *    Success or negated failure code
 *
 ****************************************************************************/

int settings_clear(void);

/****************************************************************************
 * Name: settings_create
 *
 * Description:
 *    Creates a new setting.
 *
 *    If the setting is found to exist in any of the storages, it will
 *    be loaded. Else, it will be created and the default value will be
 *    assigned.
 *
 * Input Parameters:
 *        key         - the key of the setting.
 *        type        - the type of the setting.
 *        ...         - the default value of the setting.
 *
 * Returned Value:
 *    Success or negated failure code
 *
 ****************************************************************************/

int settings_create(FAR char *key, enum settings_type_e type, ...);

/****************************************************************************
 * Name: settings_type
 *
 * Description:
 *    Gets the type of a setting.
 *
 * Input Parameters:
 *    key         - the key of the setting.
 *    type        = pointer to int for the returned setting type
 *
 * Returned Value:
 *    Success or negated failure code
 *
 ****************************************************************************/

int settings_type(FAR char *key, FAR enum settings_type_e *stype);

/****************************************************************************
 * Name: settings_get
 *
 * Description:
 *    Gets the value of a setting.
 *
 * Input Parameters:
 *    key         - the key of the setting.
 *    type        - the type of the setting
 *    ...         - pointer to store the setting value.
 *
 * Returned Value:
 *    Success or negated failure code
 *
 ****************************************************************************/

int settings_get(FAR char *key, FAR enum settings_type_e type, ...);

/****************************************************************************
 * Name: settings_set
 *
 * Description:
 *    Sets the value of a setting.
 *
 * Input Parameters:
 *    key         - the key of the setting.
 *    type        - the type of the setting
 *    ...         - the new value of the setting.
 *
 * Returned Value:
 *    Success or negated failure code
 *
 ****************************************************************************/

int settings_set(FAR char *key, FAR enum settings_type_e type, ...);

/****************************************************************************
 * Name: settings_iterate
 *
 * Description:
 *    Gets a copy of a setting at the specified position. It can be used to
 *    iterate over the settings map, by using successive values of idx.
 *
 * Input Parameters:
 *    idx         - the iteration index for the setting.
 *    setting     - pointer to return the setting value
 *
 * Returned Value:
 *    Success or negated failure code
 *
 ****************************************************************************/

int settings_iterate(int idx, FAR setting_t *setting);

/****************************************************************************
 * Name: settings_usedsize
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

int settings_usedsize(FAR storage_used_t *used);

#endif /* UTILS_SETTINGS_H_ */

