# Introduction

The settings storage can be used to store and retrieve various configurable parameters. This storage is a RAM-based map (for fast access), but one or more files can be used too (so values can persist across system reboots).

This is a thread-safe implementation. Different threads may access the settings simultaneously.

# Setting Definition

Each setting is a key / value pair.

The key is always a string, while there are many value types supported.
 
## Strings

All strings, whether they represent a key or a string value, must use a specific format. They must be ASCII, NULL-terminated strings, and they are always case-sensitive. They must obey to the following rules:

- they cannot start with a number.
- they cannot contain the characters '=', ';'.
- they cannot contain the characters '\n', '\r'.

## Setting Type

Since each setting has its own type, it is the user's responsibility to access the setting using the correct type. Some "casts" are possible (e.g. between bool and int), but generally reading with the wrong type will lead to failure.

The following types are currently supported.

- String.
- int
- byte (uint8_t)
- bool
- ip address
- float

### Setting Storage Sizes

Kconfig is used to determine the size of the various fields used:

- <CONFIG_SYSTEM_SETTINGS_MAP_SIZE>       the total number settings allowed
- <CONFIG_SYSTEM_SETTINGS_MAX_STORAGES>   the number of storage files that can be used
- <CONFIG_SYSTEM_SETTINGS_VALUE_SIZE>     the storage size of a STRING value
- <CONFIG_SYSTEM_SETTINGS_KEY_SIZE>       the size of the KEY field
- <CONFIG_SYSTEM_SETTINGS_MAX_FILENAME>   the maximum filename size

# Signal

A POSIX signal can be chosen via Kconfig and is used to send notifications when a setting has been changed. <CONFIG_SYSTEM_SETTINGS_MAX_SIGNALS> is used to determine the maximum number of signals that can be registered for the settings storage functions.

# Files

There are also various types of files that can be used. Each file type is using a different data format to parse the stored data. Every type is expected to be best suited for a specific file system or medium type, or be better performing while used with external systems. In any case, all configured files are automatically synchronized whenever a value changes.

## Cached Saves

It is possible to enable cached saves, via Kconfig, along with the time (in ms) that should elapse after the last storage value was updated before a write to the file or files should be initiated.

## Storage Types
The current storage types are defined in <enum storage_type_e> and are as follows.

### STORAGE_BINARY

Data is stored as "raw" binary. Byte, bool, int and float data are stored using the maximum number of bytes needed, as determined by the size of the union used for these settings; this will be architecture dependent.

### STORAGE_TEXT

All data is converted to ASCII characters making the storage easily human-readable.

### STORAGE_EEPROM

This is like STORAGE_BINARY with the following differences.

- When writing data to the file, only changed values are written out, to minimise the number of write cycles. This, of course, applies to other storage technologies too.
- Values are stored using the minimum number of data bytes possible, since EEPROM devices are usually small.
- A saved value is verified by a read and compare, since the write mechanism has no inherent confirmation of a correct write operation.
- Other storage types allow, in some case, settings to be re-specified (cast) between various types. To minimise the storage space used, EEPROM storage only allows this if the new storage size is the same or less that the initially created storage type.

  - This could be done, if needed, using an external function to read all data, re-cast the settings, and recreate them all from scratch.

# Usage



