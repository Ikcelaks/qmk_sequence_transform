# qmk_sequence_transform
Sequence-Transform is a user library for QMK that enables a rich declarative ruleset for transforming a sequence of keypresses into any output you would like.

Inspiration for Sequence-Transform was taken from Pascal Getreuer's innovative [Autocorrect feature](https://getreuer.info/posts/keyboards/autocorrection/index.html),
without which we would not have even conceived this idea!

# User Guide
THIS LIBRARY IS NOT FINALIZED. A user who isn't looking to get involved in development should only use this right now if they want to test things out and are
willing to deal with frequent breaking design changes.
## Setup
Here is a [working example](https://github.com/Ikcelaks/qmk_userspace/tree/main/keyboards/moonlander/keymaps/ikcelaks) of a fairly minimal keymap that is using this library.
- From a terminal with the current working directory set to your keymap directory (example: `qmk_userspace/keyboard/moonlander/keymaps/ikcelaks`), run this
command to add the library as a git submodule (no need to create a fork first):<br/>
`git submodule add https://github.com/ikcelaks/qmk_sequence_transform.git sequence_transform`
- At the end of your `rules.mk` file, add the following lines:</br>
```
        # sequence_transform setup
        mkfile_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
        
        all: st_build
        
        st_build:
        	python3 $(mkfile_dir)/sequence_transform/generator/sequence_transform_data.py
        
        include $(mkfile_dir)/sequence_transform/rules.mk
        # end sequence_transform setup
```
- Define custom keycodes for your Sequence Token keys (commonly referred to as "magic keys") consecutively. Example:
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
- Add the following to the list of includes in your `keymap.c` file.
```c
        #include "sequence_transform/sequence_transform.h"
```
- In the `process_record_user` function of your `keymap.c` file, add the following (or equivalent) (Replace `US_MAG1` with whatever your first Sequence Token key is named):<br/>
```c
        if (!process_sequence_transform(keycode, record, US_MAG1)) return false;
```

- Add this line to your `post_process_record_user` function in `keymap.c`:<br/>
```c
        void post_process_record_user(uint16_t keycode, keyrecord_t *record) {
            post_process_sequence_transform();  // Add this line
        }
```
- Copy the `sequence_transform_config_sample.json` and `sequence_transform_dict_sample.txt` files from the `./sequence_transform/generator` directory
into the base directory of your keymap (which should contain the `sequence_transform` directory as a direct subdirectory).<br/>
**Rename both files to remove the `_sample`**. They should now be named `sequence_transform_config.json` and `sequence_transform_dict.txt` respectively.
