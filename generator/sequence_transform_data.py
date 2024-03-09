# Copyright 2021 Google LLC
# Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
# Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
# Copyright 2024 QKekos <q.kekos.q@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Python program to make sequence_transform_data.h.
This program reads from a prepared dictionary file and generates a
C source file "sequence_transform_data.h"
with a serialized trie embedded as an array.

Each line of the dict file defines "sequence -> transformation" pair.
Blank lines or lines starting with comment string are ignored.

Examples:
  :ex@        -> example
  :ob@        -> obvious
  :d@         -> develop
  :d@r        -> developer
"""

import re
import textwrap
import json
from typing import Any, Dict, Iterator, List, Tuple, Callable
from datetime import date, datetime
from string import digits
from pathlib import Path
from argparse import ArgumentParser

ST_GENERATOR_VERSION = "SEQUENCE_TRANSFORM_GENERATOR_VERSION_3"

GPL2_HEADER_C_LIKE = f'''\
// Copyright {date.today().year} QMK
// SPDX-License-Identifier: GPL-2.0-or-later
'''

GENERATED_HEADER_C_LIKE = f'''\
// This was file generated on {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}.
// Do not edit this file directly!
'''

KC_A = 0x04
KC_SPC = 0x2c
KC_MINUS = 0x2D
KC_SEMICOLON = 0x33
KC_1 = 0x1E
MOD_LSFT = 0x0200
KC_MAGIC_0 = 0x0100
TRIE_MATCH_BIT = 0x8000
TRIE_BRANCH_BIT = 0x4000
qmk_digits = digits[1:] + digits[0]
OUTPUT_FUNC_1 = 1
OUTPUT_FUNC_COUNT_MAX = 7

max_backspaces = 0
S = lambda code: MOD_LSFT | code


class bcolors:
    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    PURPLE = "\033[95m"
    CYAN = "\033[96m"

    ENDC = "\033[0m"
    BOLD = "\033[1m"
    UNDERLINE = "\033[4m"


###############################################################################
def color_currying(color: str) -> Callable:
    def inner(*text: Any) -> str:
        return f"{color}{' '.join(map(str, text))}{bcolors.ENDC}"

    return inner


###############################################################################
red = color_currying(bcolors.RED)
cyan = color_currying(bcolors.CYAN)


###############################################################################
def err(*text: Any) -> str:
    return red("Error:", *text)


###############################################################################
def map_range(start: int, chars: str) -> dict[str, int]:
    return {char: start + i for i, char in enumerate(chars)}


###############################################################################
def generate_context_char_map(magic_chars, wordbreak_char) -> Dict[str, int]:
    return {
        **map_range(KC_SEMICOLON, ";'`,./"),
        **map_range(S(KC_SEMICOLON), ":\"~<>?"),
        **map_range(KC_MINUS, "-=[]\\"),
        **map_range(S(KC_MINUS), "_+\{\}|"),
        **map_range(KC_1, qmk_digits),
        **map_range(S(KC_1), "!@#$%^&*()"),
        **map_range(KC_MAGIC_0, magic_chars),

        wordbreak_char: KC_SPC,  # "Word break" character.
        **{chr(c): c + KC_A - ord('a') for c in range(ord('a'), ord('z') + 1)}
    }


###############################################################################
def quiet_print(*args, **kwargs):
    if IS_QUIET:
        return

    print(*args, **kwargs)


###############################################################################
def generate_output_func_char_map(output_func_chars) -> Dict[str, int]:
    if len(output_func_chars) > OUTPUT_FUNC_COUNT_MAX:
        output_chars = str(len(output_func_chars))

        raise SystemExit(
            f'{err()} More than {OUTPUT_FUNC_COUNT_MAX} ({cyan(output_chars)})'
            f' output_func_chars were listed {output_func_chars}'
        )

    return dict([
        (char, OUTPUT_FUNC_1 + i)
        for i, char in enumerate(output_func_chars)
    ])


###############################################################################
def parse_file(
    file_name: str, char_map: Dict[str, int],
    separator: str, comment: str
) -> List[Tuple[str, str]]:
    """Parses sequence dictionary file.
    Each line of the file defines one "sequence -> transformation" pair.
    Blank lines or lines starting with the comment string are ignored.
    The function validates that sequences only have characters a-z.
    Overlapping sequences are matched to the longest valid match.
    """

    file_lines = parse_file_lines(file_name, separator, comment)
    context_set = set()
    rules = []

    for line_number, context, completion in file_lines:
        if context in context_set:
            print(
                f'{err("line", line_number)}: '
                f'Ignoring duplicate sequence: "{cyan(context)}"'
            )
            continue

        # Check that `context` is valid.
        if not all([(c in char_map) for c in context[:-1]]):
            raise SystemExit(
                f'{err("line", line_number)}: '
                f'sequence "{cyan(context)}" has invalid characters'
            )

        if len(context) > 127:
            raise SystemExit(
                f'{err("line", line_number)}:'
                f'Sequence exceeds 127 chars: "{cyan(context)}"'
            )

        rules.append((context, completion))
        context_set.add(context)

    return rules


###############################################################################
def make_trie(
    seq_dict: List[Tuple[str, str]],
    output_func_char_map: Dict[str, int]
) -> Dict[str, tuple[str, dict]]:
    """Makes a trie from the sequences, writing in reverse."""
    trie = {}

    for context, correction in seq_dict:
        node = trie

        if correction[-1] in output_func_char_map:
            output_func = output_func_char_map[correction[-1]]
            target = correction[:len(correction)-1]

        else:
            output_func = 0
            target = correction

        for letter in context[::-1]:
            node = node.setdefault(letter, {})

        node['MATCH'] = (context, {
            'TARGET': target,
            'RESULT': {
                'BACKSPACES': -1,
                'FUNC': output_func,
                'OUTPUT': ""
            }
        })

    return trie


###############################################################################
def complete_trie(trie: Dict[str, Any], wordbreak_char: str) -> set[str]:
    outputs = set()

    def complete_node(context, completion):
        nonlocal outputs

        back_context = []
        expanded_context = []

        for c in context[:-1]:
            back_context.append(c)
            expanded_context.append(c)
            match = get_trie_result(back_context)

            if not match:
                match = get_trie_result(expanded_context)

            if match:
                match_backspaces = match['RESULT']['BACKSPACES']
                match_output = match['RESULT']['OUTPUT']

                del expanded_context[-(match_backspaces + 1):]
                expanded_context.extend(match_output)
                # quiet_print(c, expanded_context)

        if expanded_context and expanded_context[0] == wordbreak_char:
            del expanded_context[0]

        target = completion['TARGET']

        i = 0  # Make the autocorrection data for this entry and serialize it.
        while (
            i < min(len(expanded_context), len(target)) and
            expanded_context[i] == target[i]
        ):
            i += 1

        backspaces = len(expanded_context) - i
        output = target[i:].replace(wordbreak_char, " ")

        outputs.add(output)
        completion['RESULT']['BACKSPACES'] = backspaces
        completion['RESULT']['OUTPUT'] = output

    def get_trie_result(buffer: List[str]) -> dict[str, dict[str]]:
        longest_match = {}
        trienode = trie

        for c in reversed(buffer):
            if c not in trienode:
                break

            # quiet_print(c)
            trienode = trienode[c]

            if 'MATCH' in trienode:
                context, completion = trienode['MATCH']

                if completion['RESULT']['BACKSPACES'] == -1:
                    complete_node(context, completion)

                longest_match = completion

        return longest_match

    def traverse_trienode(trinode: Dict[str, Any]):
        for key, value in trinode.items():
            if key == 'MATCH':
                context, completion = value

                if completion['RESULT']['BACKSPACES'] == -1:
                    complete_node(context, completion)
            else:
                traverse_trienode(value)

    traverse_trienode(trie)
    return outputs


###############################################################################
def generate_matches(pattern) -> list[tuple[str, str]]:
    patterns = [("", pattern)]

    for i, pattern in enumerate([pattern]):
        groups = re.findall(r"\((?:\w\|?)+\)\??", pattern)

        if not groups:
            return patterns

        patterns = []
        match = groups[0]
        elements = re.findall("\w+", match)

        patterns.extend([
            (element, pattern.replace(match, element))
            for element in elements
        ])

        if "?" in match:
            patterns.append(("", pattern.replace(match, "")))

    return patterns


###############################################################################
def parse_tokens(tokens: List[str]) -> Iterator[Tuple[int, str, str]]:
    full_sequence, transform = tokens

    for token, sequence in generate_matches(full_sequence):
        yield sequence, transform.replace(r"\1", token)


###############################################################################
def parse_file_lines(
    file_name: str, separator: str, comment: str
) -> Iterator[Tuple[int, str, str]]:
    """Parses lines read from `file_name` into context-correction pairs."""
    line_number = 0

    with open(file_name, 'rt', encoding="utf-8") as file:
        for line in file:
            line_number += 1
            line = line.strip()

            if line and line.find(comment) != 0:
                # Parse syntax "sequence -> transformation".
                # Using strip to ignore indenting.
                tokens = [token.strip() for token in line.split(separator, 1)]

                if len(tokens) != 2 or not tokens[0]:
                    raise SystemExit(
                        f'{err(line_number)}: Invalid syntax: "{red(line)}"'
                    )

                for context, correction in parse_tokens(tokens):
                    yield line_number, context, correction


###############################################################################
def serialize_outputs(
    outputs: set[str]
) -> Tuple[List[int], Dict[str, int], int]:
    quiet_print(sorted(outputs, key=len, reverse=True))
    completions_str = ''
    completions_map = {}
    completions_offset = 0
    max_completion_len = 0

    for output in sorted(outputs, key=len, reverse=True):
        max_completion_len = max(max_completion_len, len(output))
        i = completions_str.find(output)

        if i == -1:
            quiet_print(f'{output} added at {completions_offset}')
            completions_map[output] = completions_offset
            completions_str += output
            completions_offset += len(output)

        else:
            quiet_print(f'{output} found at {i}')
            completions_map[output] = i

    quiet_print(completions_str)

    return (
        list(bytes(completions_str, 'ascii')),
        completions_map,
        max_completion_len
    )


###############################################################################
def serialize_trie(
    char_map: Dict[str, int], trie: Dict[str, Any],
    completions_map: Dict[str, int]
) -> List[int]:
    """Serializes trie in a form readable by the C code.

    Returns:
    List of 16bit ints in the range 0-64k.
    """
    table = []

    # Traverse trie in depth first order.
    def traverse(trie_node):
        global max_backspaces
        if 'MATCH' in trie_node:  # Handle a MATCH trie node.
            sequence, completion = trie_node['MATCH']
            backspaces = completion['RESULT']['BACKSPACES']
            max_backspaces = max(max_backspaces, backspaces)
            func = completion['RESULT']['FUNC']
            output = completion['RESULT']['OUTPUT']
            output_index = completions_map[output]

            # 2 bits (16,15) are used for node type
            code = TRIE_MATCH_BIT + TRIE_BRANCH_BIT * (len(trie_node) > 1)

            # 3 bits (14..12) are used for special function
            assert 0 <= func < 8
            code += func << 11

            # 4 bits (11..8) are used for backspaces
            assert 0 <= backspaces < 16
            code += backspaces << 7

            # 7 bits (bits 7..1) are used for completion_len
            completion_len = len(output)
            assert 0 <= completion_len < 128
            code += completion_len

            # First output word stores coded info
            # Second stores completion data offset index
            data = [code, output_index]
            # quiet_print(f'{err(0)} Data "{cyan(data)}"')
            del trie_node['MATCH']

        else:
            data = []

        if len(trie_node) == 0:
            entry = {'data': data, 'links': [], 'uint16_offset': 0}
            table.append(entry)

        elif len(trie_node) == 1:  # Handle trie node with a single child.
            c, trie_node = next(iter(trie_node.items()))
            entry = {'data': data, 'chars': c, 'uint16_offset': 0}

            # It's common for a trie to have long chains of single-child nodes.

            # We find the whole chain so that
            # we can serialize it more efficiently.
            while len(trie_node) == 1 and 'MATCH' not in trie_node:
                c, trie_node = next(iter(trie_node.items()))
                entry['chars'] += c

            table.append(entry)
            entry['links'] = [traverse(trie_node)]

        else:  # Handle trie node with multiple children.
            entry = {
                'data': data,
                'chars': ''.join(sorted(trie_node.keys())),
                'uint16_offset': 0
            }

            table.append(entry)
            entry['links'] = [traverse(trie_node[c]) for c in entry['chars']]

        # quiet_print(f'{err(0)} Data "{cyan(entry["data"])}"')
        return entry

    traverse(trie)
    # quiet_print(f'{err(0)} Data "{cyan(table)}"')

    def serialize(node: Dict[str, Any]) -> List[int]:
        data = node['data']
        # quiet_print(f'{err(0)} Serialize Data "{cyan(data)}"')

        if not node['links']:  # Handle a leaf table entry.
            return data

        elif len(node['links']) == 1:  # Handle a chain table entry.
            return data + [char_map[c] for c in node['chars']] + [0]

        else:  # Handle a branch table entry.
            links = []

            for c, link in zip(node['chars'], node['links']):
                links += [
                    char_map[c] | (0 if links else TRIE_BRANCH_BIT)
                ] + encode_link(link)

            return data + links + [0]

    uint16_offset = 0

    # To encode links, first compute byte offset of each entry.
    for table_entry in table:
        table_entry['uint16_offset'] = uint16_offset
        uint16_offset += len(serialize(table_entry))

        assert 0 <= uint16_offset <= 0xffff

    # Serialize final table.
    return [b for node in table for b in serialize(node)]


###############################################################################
def encode_link(link: Dict[str, Any]) -> List[int]:
    """Encodes a node link as two bytes."""
    uint16_offset = link['uint16_offset']

    if not (0 <= uint16_offset <= 0xffff):
        raise SystemExit(
            f'{err()} The transforming table is too large, '
            f'a node link exceeds 64KB limit. '
            f'Try reducing the transforming dict to fewer entries.'
        )

    return [uint16_offset]


###############################################################################
def sequence_len(node: Tuple[str, str]) -> int:
    return len(node[0])


###############################################################################
def transform_len(node: Tuple[str, str]) -> int:
    return len(node[1])


###############################################################################
def byte_to_hex(b: int) -> str:
    return f'0x{b:02X}'


###############################################################################
def uint16_to_hex(b: int) -> str:
    return f'0x{b:04X}'


###############################################################################
def create_test_rule_c_string(
    char_map: Dict[str, int],
    sequence: str,
    transform: str
) -> str:
    """ returns a string with the following format: 
        { "transform", (uint16_t[4]){ 0x1234, 0x1234, 0x1234, 0} },
    """
    # we don't want any utf8 symbols in transformation string here
    transform_dict = {
        "\\": "\\\\",
        WORDBREAK_CHAR: " ",
        **{sym: char for sym, char in zip(MAGIC_CHARS, SEQ_TOKENS_ASCII)}
    }
    for (i, j) in transform_dict.items():
        transform = transform.replace(i, j)
    seq_ints = [char_map[c] for c in sequence] + [0]
    seq_int_str = ', '.join(map(uint16_to_hex, seq_ints))
    res = f'    {{ "{transform}", (uint16_t[{len(seq_ints)}]){{ {seq_int_str} }} }},'
    return res


###############################################################################
def generate_sequence_transform_data(data_header_file, test_header_file):
    char_map = generate_context_char_map(MAGIC_CHARS, WORDBREAK_CHAR)
    output_func_char_map = generate_output_func_char_map(OUTPUT_FUNC_CHARS)

    seq_dict = parse_file(RULES_FILE, char_map, SEP_STR, COMMENT_STR)
    trie = make_trie(seq_dict, output_func_char_map)
    outputs = complete_trie(trie, WORDBREAK_CHAR)
    quiet_print(json.dumps(trie, indent=4))

    s_outputs = serialize_outputs(outputs)
    completions_data, completions_map, max_completion_len = s_outputs

    trie_data = serialize_trie(char_map, trie, completions_map)

    assert all(0 <= b <= 0xffff for b in trie_data)
    assert all(0 <= b <= 0xff for b in completions_data)

    min_sequence = min(seq_dict, key=sequence_len)[0]
    max_sequence = max(seq_dict, key=sequence_len)[0]
    max_transform = max(seq_dict, key=transform_len)[1]

    # Build the sequence_transform_data.h file.
    transformations = []
    test_rules_c_strings = []

    for sequence, transformation in seq_dict:
        # Don't add rules with transformation functions to test header for now
        if transformation[-1] not in output_func_char_map:
            test_rule = create_test_rule_c_string(char_map, sequence, transformation)
            test_rules_c_strings.append(test_rule)
        transformation = transformation.replace("\\", "\\ [escape]")
        sequence = f"{sequence:<{len(max_sequence)}}"
        transformations.append(f'//    {sequence} -> {transformation}')

    header_lines = [
        GPL2_HEADER_C_LIKE,
        GENERATED_HEADER_C_LIKE,
        '#pragma once',
    ]

    tranformations_lines = [
        f'// Sequence Transformation dictionary with longest match semantics',
        f'// Dictionary ({len(seq_dict)} entries):',
        *transformations,
    ]

    # token symbols stored as utf8 strings
    sym_array_str = ", ".join(map(lambda c: f'"{c}"', MAGIC_CHARS))
    st_seq_tokens = f'static const char *st_seq_tokens[] = {{ {sym_array_str} }};'
    st_wordbreak_token = f'static const char *st_wordbreak_token = "{WORDBREAK_CHAR}";'
    # ascii versions
    char_array_str = ", ".join(map(lambda c: f"'{c}'", SEQ_TOKENS_ASCII))
    st_seq_tokens_ascii = f'static const char st_seq_tokens_ascii[] = {{ {char_array_str} }};'
    st_wordbreak_ascii = f"static const char st_wordbreak_ascii = '{WORDBREAK_ASCII}';"

    trie_stats_lines = [
        f'#define {ST_GENERATOR_VERSION}',
        '',
        f'#define SPECIAL_KEY_TRIECODE_0 {uint16_to_hex(KC_MAGIC_0)}',
        f'#define SEQUENCE_MIN_LENGTH {len(min_sequence)} // "{min_sequence}"',
        f'#define SEQUENCE_MAX_LENGTH {len(max_sequence)} // "{max_sequence}"',
        f'#define TRANSFORM_MAX_LEN {len(max_transform)} // "{max_transform}"',
        f'#define COMPLETION_MAX_LENGTH {max_completion_len}',
        f'#define MAX_BACKSPACES {max_backspaces}',
        f'#define DICTIONARY_SIZE {len(trie_data)}',
        f'#define COMPLETIONS_SIZE {len(completions_data)}',
        f'#define SEQUENCE_TRANSFORM_COUNT {len(MAGIC_CHARS)}',
        '',
        st_seq_tokens_ascii,
        st_wordbreak_ascii
    ]

    trie_data_lines = [
        'static const uint16_t '
        'sequence_transform_data[DICTIONARY_SIZE] PROGMEM = {',

        textwrap.fill(
            '    %s' % (', '.join(map(uint16_to_hex, trie_data))),
            width=135, subsequent_indent='    '
        ),
        '};\n',

        'static const uint8_t '
        'sequence_transform_completions_data[COMPLETIONS_SIZE] PROGMEM = {',

        textwrap.fill(
            '    %s' % (', '.join(map(byte_to_hex, completions_data))),
            width=100, subsequent_indent='    '
        ),
        '};\n',
    ]

    # Write data header file
    sequence_transform_data_h_lines = [
        *header_lines,
        '',
        *trie_stats_lines,
        '',
        *tranformations_lines,
        '',
        *trie_data_lines,
    ]
    with open(data_header_file, "w", encoding="utf-8") as file:
        file.write("\n".join(sequence_transform_data_h_lines))

    # Write test header file
    sequence_transform_test_h_lines = [
        *header_lines,
        '',
        st_seq_tokens,
        st_wordbreak_token,
        '',
        'typedef struct {',
        '   const char * const      transform_str;',
        '   const uint16_t * const  seq_keycodes;',
        '} st_test_rule_t;',
        '',
        'static const st_test_rule_t st_test_rules[] = {',
        *test_rules_c_strings,
        '    { 0, 0 }',
        '};'
    ]
    with open(test_header_file, "w", encoding="utf-8") as file:
        file.write("\n".join(sequence_transform_test_h_lines))


###############################################################################
if __name__ == '__main__':
    parser = ArgumentParser()

    parser.add_argument(
        "-c", "--config", type=str,
        help="config file path", default="../../sequence_transform_config.json"
    )

    parser.add_argument("-q", "--quiet", action="store_true")
    cli_args = parser.parse_args()

    THIS_FOLDER = Path(__file__).parent

    data_header_file = THIS_FOLDER / "../sequence_transform_data.h"
    test_header_file = THIS_FOLDER / "../sequence_transform_test.h"
    config_file = THIS_FOLDER / cli_args.config
    config = json.load(open(config_file, 'rt', encoding="utf-8"))

    try:
        SEQ_TOKENS_ASCII = config['seq_tokens_ascii']
        WORDBREAK_ASCII = config['wordbreak_ascii']
        MAGIC_CHARS = config['magic_chars']
        OUTPUT_FUNC_CHARS = config['output_func_chars']
        WORDBREAK_CHAR = config['wordbreak_char']
        COMMENT_STR = config['comment_str']
        SEP_STR = config['separator_str']
        RULES_FILE = THIS_FOLDER / "../../" / config['rules_file_name']
    except KeyError as e:
        raise KeyError(f"Incorrect config! {e} key is missing.")

    IS_QUIET = config.get("quiet", True)

    if cli_args.quiet:
        IS_QUIET = True

    generate_sequence_transform_data(data_header_file, test_header_file)
