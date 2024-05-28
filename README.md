# QMK Sequence Transform
Sequence-Transform is a user library for QMK that enables a rich declarative ruleset for transforming a sequence of keypresses into any output you would like.

Inspiration for Sequence-Transform was taken from Pascal Getreuer's innovative [Autocorrect feature](https://getreuer.info/posts/keyboards/autocorrection/index.html),
without which we would not have even conceived this idea!

# User Guide
THIS LIBRARY IS NOT FINALIZED. A user who isn't looking to get involved in development should only use this right now if they want to test things out and are
willing to deal with frequent breaking design changes.
## Setup
Here is a [working example](https://github.com/Ikcelaks/qmk_userspace/tree/main/keyboards/moonlander/keymaps/ikcelaks) of a fairly minimal keymap that is using this library.
### Step 1
From a terminal with the current working directory set to your keymap directory (example: `qmk_userspace/keyboard/moonlander/keymaps/ikcelaks`), run this
command to add the library as a git submodule (no need to create a fork first):<br/>
`git submodule add https://github.com/ikcelaks/qmk_sequence_transform.git sequence_transform`

### Step 2
At the end of your `rules.mk` file, add the following lines:</br>
```makefile
# sequence_transform setup
mkfile_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
include $(mkfile_dir)/sequence_transform/rules.mk
# end sequence_transform setup
```

### Step 3
Define custom keycodes for your Sequence Token keys (commonly referred to as "magic keys") consecutively. Example:
```c
enum custom_keycodes {
    US_MAG1 = SAFE_RANGE,
    US_MAG2,
    US_MAG3,
    US_MAG4,
    US_D_UND, // other custom keycodes start here
    US_QUOT_S,
};
```

### Step 4
Add the following to the list of includes in your `keymap.c` file.
```c
#include "sequence_transform/sequence_transform.h"
```

### Step 5
In the `process_record_user` function of your `keymap.c` file, add the following (or equivalent) (Replace `US_MAG1` with whatever your first Sequence Token key is named):<br/>
```c
if (!process_sequence_transform(keycode, record, US_MAG1)) return false;
```

### Step 6
Add this line to your `post_process_record_user` function in `keymap.c`:<br/>
```c
void post_process_record_user(uint16_t keycode, keyrecord_t *record) {
    post_process_sequence_transform();  // Add this line
}
```

### Step 7
Add this line to your `matrix_scan_user` function in `keymap.c`:<br/>
```c
void matrix_scan_user(void)
{
    sequence_transform_task();  // Add this line
}
```

### Step 8
Copy the `sequence_transform_config_sample.json` and `sequence_transform_dict_sample.txt` files from the `./sequence_transform/generator` directory
into the base directory of your keymap (which should contain the `sequence_transform` directory as a direct subdirectory).<br/>
**Rename both files to remove the `_sample`**. They should now be named `sequence_transform_config.json` and `sequence_transform_dict.txt` respectively.

## Configuration
All configuration is done by modifying the `sequence_transform_config.json` and `sequence_transform_dict.txt` files that you copied
into your keymap root folder. **DO NOT** make any changes to any files in the `sequence_transform` directory or its sub-directories.
### Generator Configuration File `sequence_transform_confg.json`
This file is used to tell the rules generator script how to interpret your `sequence_transform_dict.txt` file.
A full description of each setting is provided in the Wiki (TODO).

**IMPORTANT NOTE:** The number of `Sequence Token keys` defined in [step 3](#step-3) of the setup, **must** match the number of `sequence_token_symbols` defined in the config.

### Rule Set File `sequence_transform_dict.txt`
This file contains a list of all the rules that the generator script will encode into the trie structure.
A full explanation of how rules are constructed, how they manipulate the result of your keypresses, and how they interact with each other is found in the Wiki (TODO).

### Add Sequence Tokens to your keymap
You should add the custom keys you defined in [step 3](#step-3) of the setup to your keymap. These custom keys will be matched one to one with the `sequence_token_symbols` defined in your `sequence_transform_config.json` file. That is, the custom key that you pass to `process_sequence_transform` in [step 5](#step-5) of the setup will correspond to the first symbol defined in `sequence_token_symbols`, and each following custom key will be matched with the next symbol. (This is why you **must** have the same number of each).

Symbols chosen can be any utf-8 symbol you like. The sample config and dictionary use a pointing finger and thumb emoji to aid in remembering which `Sequence Token key` is being used, which you may find helpful.

## Building
No special steps are required to build your firmware while using this library! Your rule set dictionary is automatically built into the 
required datastructure if necessary everytime you re-compile your firmware. This is accomplished by the lines added to your `rules.mk` file in [step 2](#step-2) of the setup.

## Testing
Sequence Transform provides an offline `tester` utility that will allow you to test changes to your rules without needing to flash a new firmware to your keyboard. This tool was instrumental during the development process, but we think you will enjoy it too as you explore new and increasingly complex rules to add to your arsenal. We have tried very hard to minimize the complexities of writing and understanding rules, but even the developers sometimes write rules that work differently than envisioned.

Instructions for building and using the `tester` utility are found in the Wiki. (TODO)
