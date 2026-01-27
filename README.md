# QMK Sequence Transform
Sequence-Transform is a user library for QMK that enables a rich declarative ruleset for transforming a sequence of keypresses into any output you would like.

Inspiration for Sequence-Transform was taken from Pascal Getreuer's innovative [Autocorrect feature](https://getreuer.info/posts/keyboards/autocorrection/index.html),
without which we would not have even conceived this idea!

# User Guide
THIS LIBRARY IS NOT FINALIZED. A user who isn't looking to get involved in development should only use this right now if they want to test things out and are
willing to deal with frequent breaking design changes.
## Installation
This library is now implemented as a QMK [Community Module](https://docs.qmk.fm/features/community_modules).

This [working example](https://github.com/Ikcelaks/qmk_userspace/tree/main/keyboards/moonlander/keymaps/ikcelaks) is a fairly minimal keymap that is using this library.

### Step 1: Determine your QMK directory
Future steps will depend on a path to your QMK directory (referred to as `{qmk_path}`). This will vary depending on how you installed QMK.

If you're using a QMK external userspace, the directory will be named `qmk_userspace`, otherwise, it will be named `qmk_firmware`.

> [!Example]
> If you're using the external userspace installed directly into your home directory on either Linux or WSL, your `qmk_path` will be `~/qmk_userspace`

### Step 2: Clone the library into your `modules` folder
In a terminal run the following commands to clone (first replace `{qmk_path}` with the path determined in step 1):
```bash
cd {qmk_path}
mkdir -p modules
git submodule add https://github.com/ikcelaks/qmk_sequence_transform.git modules/ikcelaks/sequence_transform
git submodule update --init --recursive
```

### Step 3: Add `ikcelaks/sequence_transform` to your `keymap.json`
Add `"ikcelaks/sequence_transform"` to the `modules` array in your `keymap.json` file.

If you don't yet have a `keymap.json` file, create one and add the following contents:
```json
{
    "modules": ["ikcelaks/sequence_transform"]
}
```

## Configuration
All configuration is done by modifying the `sequence_transform_config.json` and `sequence_transform_dict.txt` files that you copied
into your keymap root folder. **DO NOT** make any changes to any files in the `sequence_transform` directory or its sub-directories.
### Generator Configuration File `sequence_transform_confg.json`
This file is used to tell the rules generator script how to interpret your `sequence_transform_dict.txt` file.
A full description of each setting is provided in the Wiki (TODO).

> [!IMPORTANT]
> The number of `Sequence Token keys` defined in [step 3](#step-3) of the setup, **must** match the number of `sequence_token_symbols` defined in the config.

### Rule Set File `sequence_transform_dict.txt`
This file contains a list of all the rules that the generator script will encode into the trie structure.
A full explanation of how rules are constructed, how they manipulate the result of your keypresses, and how they interact with each other is found in the Wiki (TODO).

The symbols that you will need to use when constructing your rules are included at the top of rules dictionary file when it's created (see [step 8](#step-8)).

> [!TIP]
> For ideas on what rules you can write, take a look at the [sample dictionary](generator/sequence_transform_dict_sample.txt).

### Add Sequence Tokens to your keymap
This library defines five custom keycodes (`ST_MAG1, ST_MAG2 .. ST_MAG5`) to be used as Sequence Tokens. These keys will be matched one to one with the `sequence_token_symbols` defined in your `sequence_transform_config.json` file. That is, `ST_MAG1` will correspond to the first symbol defined in `sequence_token_symbols`, and each following `ST_MAGX` will be matched with the next symbol.

Symbols chosen can be any utf-8 symbol you like. The sample config and dictionary use a pointing finger and thumb emoji to aid in remembering which `Sequence Token key` is being used, which you may find helpful.

## Building
No special steps are required to build your firmware while using this library! Your rule set dictionary is automatically built into the required datastructure if necessary everytime you re-compile your firmware. This is accomplished by the lines added to your keymap's `rules.mk` file in [step 2](#step-2) of the setup.

## Multiple Rule Sets
You can split your rule sets into multiple files. The rules in all files are merged together before being processed, 
so they must use the same symbols. The main use case is to split out rules with private or personal information into a 
file that is ignored by Git.

Follow these steps to create a second private rules file and add it to the `.gitignore`:
- Add `"sequence_transform_dict_private.txt"` to the `rules_file_name_list` in your `sequence_transform_config.json` file. The final result should look something like this:
    ```json
    "rules_file_name_list": [
        "sequence_transform_dict.txt",
        "sequence_transform_dict_private.txt"
    ]
    ```
- Build your firmware as normal, which will automatically generate a new rule set file named `sequence_transform_dict_private.txt`.
- Add `*_private.txt` to the `.gitignore` file of the repo you store your layout in. This is NOT the `.gitignore` file contained in the `sequence_transform` folder (that only manages files in the Sequence Transform library). The `.gitignore` file you want to change is probably at the root of your fork of the `qmk_userspace` or `qmk_firmware` repo.

## Testing
Sequence Transform provides an offline `tester` utility that will allow you to test changes to your rules without needing to flash a new firmware to your keyboard. This tool was instrumental during the development process, but we think you will enjoy it too as you explore new and increasingly complex rules to add to your arsenal. We have tried very hard to minimize the complexities of writing and understanding rules, but even the developers sometimes write rules that work differently than envisioned.

Instructions for building and using the `tester` utility are found in the Wiki. (TODO)
