# This file should be included in user layout's rules.mk file
all: st_build

st_build:
	python3 $(mkfile_dir)/sequence_transform/generator/sequence_transform_data.py

LIB_SRC += sequence_transform/sequence_transform.c
LIB_SRC += sequence_transform/utils.c
LIB_SRC += sequence_transform/trie.c
LIB_SRC += sequence_transform/keybuffer.c
LIB_SRC += sequence_transform/cursor.c
LIB_SRC += sequence_transform/key_stack.c
LIB_SRC += sequence_transform/triecodes.c
LIB_SRC += sequence_transform/predicates.c
LIB_SRC += sequence_transform/st_debug.c
