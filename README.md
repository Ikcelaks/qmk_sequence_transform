# qmk_sequence_transform
Sequence-Transform is a user library for QMK that enables a rich declarative ruleset for transforming a sequence of keypresses into any output you would like.

# Developer Guide
## Setup Environment
- Fork the Ikcelaks/qmk_sequence_transform repo
- From a terminal with the current working directory set to your keymap directory (example: `qmk_userspace/keyboard/moonlander/keymaps/ikcelaks`), run this
command to add the library as a git submodule:<br/>
`git submodule add git@github.com:Ikcelaks/qmk_sequence_transform.git sequence_transform`
- Wherever you are defining the custom keycodes for your keymap, make sure that your magic / special keys are defined consecutively and add the following define:
`#define SEQUENCE_TRANSFORM_SPECIAL_KEY_0 {US_SPEC1}` (where `{US_SPEC1}` is whatever name you gave to the first of the consecutive special key keycodes).
- In the `process_record_user` function of your `keymap.c` file, add the following (or equivalent):<br/>
```if (!process_context_magic(keycode, record)) return false;```

## Make Pull Request
These instructions are for what I believe is the most flexible way to make Pull Requests.
- Create a branch in the sequence_transform repo. Your editor will probably fascilitate this, but you can also run the following command in a terminal in the
`sequence_transform` directory in your keymap directory:<br/>
`git checkout -b [new_branch_name]`
- Make your changes in that branch, commit them, and publish the branch to your GitHub fork.
- In GitHub, you should see a banner message suggesting that you create a Pull Request with your new branch. Once you're ready for others to look at your code,
click the button to create the PR.
