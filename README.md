# qmk_sequence_transform
Sequence-Transform is a user library for QMK that enables a rich declarative ruleset for transforming a sequence of keypresses into any output you would like.

# Developer Guide
## Setup Environment
- Fork the Ikcelaks/qmk_sequence_transform repo
- From a terminal with the current working directory set to your keymap directory (example: `qmk_userspace/keyboard/moonlander/keymaps/ikcelaks`), run this
command to add the library as a git submodule:<br/>
`git submodule add git@github.com:{Your Github Username}/qmk_sequence_transform.git sequence_transform`
- Add ```#include "sequence_transform/sequence_transform.h"``` to your list of includes.
- Wherever you are defining the custom keycodes for your keymap, make sure that your magic / special keys are defined consecutively and add the following define:
`#define SEQUENCE_TRANSFORM_SPECIAL_KEY_0 {US_SPEC1}` (where `{US_SPEC1}` is whatever name you gave to the first of the consecutive special key keycodes).
- In the `process_record_user` function of your `keymap.c` file, add the following (or equivalent):<br/>
```if (!process_context_magic(keycode, record)) return false;```
- Add these lines to your `rules.mk` file:<br/>
```
        SRC += sequence_transform/sequence_transform.c
        SRC += sequence_transform/utils.c
```

## Make Pull Request
These instructions are for what I believe is the most flexible way to make Pull Requests.
- Create a branch in the sequence_transform repo. Your editor will probably fascilitate this, but you can also run the following command in a terminal in the
`sequence_transform` directory in your keymap directory:<br/>
`git checkout -b [new_branch_name]`
- Make your changes in that branch, commit them, and publish the branch to your GitHub fork.
- In GitHub, you should see a banner message suggesting that you create a Pull Request with your new branch. Once you're ready for others to look at your code,
click the button to create the PR.

# User Guide
THIS LIBRARY IS NOT FINALIZED. A user who isn't looking to get involved in development should only use this right now if they want to test things out and are
willing to deal with frequent breaking design changes.
- From a terminal with the current working directory set to your keymap directory (example: `qmk_userspace/keyboard/moonlander/keymaps/ikcelaks`), run this
command to add the library as a git submodule (no need to create a fork first):<br/>
`git submodule add https://github.com/ikcelaks/qmk_sequence_transform.git sequence_transform`
- Wherever you are defining the custom keycodes for your keymap, make sure that your magic / special keys are defined consecutively and add the following define:
`#define SEQUENCE_TRANSFORM_SPECIAL_KEY_0 {US_SPEC1}` (where `{US_SPEC1}` is whatever name you gave to the first of the consecutive special key keycodes).
- Add ```#include "sequence_transform/sequence_transform.h"``` to your list of includes.
- In the `process_record_user` function of your `keymap.c` file, add the following (or equivalent):<br/>
```if (!process_context_magic(keycode, record)) return false;```
- Create a config file and rules file for the generator. DETAILED INSTRUCTIONS NOT YET AVAILABLE, because this is not finalized yet.
- Add these lines to your `rules.mk` file:<br/>
```
        SRC += sequence_transform/sequence_transform.c
        SRC += sequence_transform/utils.c
```
