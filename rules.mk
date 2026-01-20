# This file should be included in user layout's rules.mk file
all: st_build

st_build:
	python3 $(MODULE_PATH_SEQUENCE_TRANSFORM)/generator/sequence_transform_data.py --keymappath $(KEYMAP_PATH) --config $(KEYMAP_PATH)/sequence_transform_config.json

LIB_SRC += utils.c
LIB_SRC += trie.c
LIB_SRC += keybuffer.c
LIB_SRC += cursor.c
LIB_SRC += key_stack.c
LIB_SRC += triecodes.c
LIB_SRC += predicates.c
LIB_SRC += st_debug.c
