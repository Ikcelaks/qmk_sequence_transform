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
TRIECODE_SEQUENCE_TOKEN_0 = 0x0100
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
def map_range(start: int, symbols: str) -> dict[str, int]:
    return {symbol: start + i for i, symbol in enumerate(symbols)}


###############################################################################
def generate_sequence_symbol_map(seq_tokens, wordbreak_symbol) -> Dict[str, int]:
    return {
        **map_range(KC_SEMICOLON, ";'`,./"),
        **map_range(S(KC_SEMICOLON), ":\"~<>?"),
        **map_range(KC_MINUS, "-=[]\\"),
        **map_range(S(KC_MINUS), "_+\{\}|"),
        **map_range(KC_1, qmk_digits),
        **map_range(S(KC_1), "!@#$%^&*()"),
        **map_range(TRIECODE_SEQUENCE_TOKEN_0, seq_tokens),

        wordbreak_symbol: KC_SPC,  # "Word break" symbol.
        **{chr(c): c + KC_A - ord('a') for c in range(ord('a'), ord('z') + 1)}
    }


###############################################################################
def quiet_print(*args, **kwargs):
    if IS_QUIET:
        return

    print(*args, **kwargs)


###############################################################################
def generate_output_func_symbol_map(output_func_symbols) -> Dict[str, int]:
    if len(output_func_symbols) > OUTPUT_FUNC_COUNT_MAX:
        output_symbols_count_str = str(len(output_func_symbols))

        raise SystemExit(
            f'{err()} More than {OUTPUT_FUNC_COUNT_MAX} ({cyan(output_symbols_count_str)})'
            f' output_func_symbols were listed {output_func_symbols}'
        )

    return dict([
        (symbol, OUTPUT_FUNC_1 + i)
        for i, symbol in enumerate(output_func_symbols)
    ])


###############################################################################
def parse_file(
    file_name: str, symbol_map: Dict[str, int],
    separator: str, comment: str
) -> List[Tuple[str, str]]:
    """Parses sequence dictionary file.
    Each line of the file defines one "sequence -> transformation" pair.
    Blank lines or lines starting with the comment string are ignored.
    The function validates that sequences only have characters a-z.
    Overlapping sequences are matched to the longest valid match.
    """

    file_lines = parse_file_lines(file_name, separator, comment)
    sequence_set = set()
    duplicated_rules = []
    rules = []

    for line_number, sequence, transform in file_lines:
        if sequence in sequence_set:
            duplicated_rules.append(
                f'{err("line", line_number)}: '
                f'Duplicate sequence: "{cyan(sequence)}"'
            )

        # Check that `sequence` is valid.
        if not all([(c in symbol_map) for c in sequence[:-1]]):
            raise SystemExit(
                f'{err("line", line_number)}: '
                f'sequence "{cyan(sequence)}" has invalid characters'
            )

        if len(sequence) > 127:
            raise SystemExit(
                f'{err("line", line_number)}:'
                f'Sequence exceeds 127 chars: "{cyan(sequence)}"'
            )

        if IMPLICIT_TRANSFORM_LEADING_WORDBREAK and sequence.startswith(WORDBREAK_SYMBOL):
            transform = WORDBREAK_SYMBOL + transform

        rules.append((sequence, transform))
        sequence_set.add(sequence)

    if duplicated_rules:
        raise SystemExit("\n".join(duplicated_rules))

    return rules


###############################################################################
def make_sequence_trie(
    seq_list: List[Tuple[str, str]],
    output_func_symbol_map: Dict[str, int]
) -> Dict[str, tuple[str, dict]]:
    """Makes a trie from the sequences, writing in reverse."""
    trie = {}

    for sequence, transform in seq_list:
        node = trie

        if transform[-1] in output_func_symbol_map:
            output_func = output_func_symbol_map[transform[-1]]
            target = transform[:len(transform)-1]

        else:
            output_func = 0
            target = transform

        for letter in sequence[::-1]:
            node = node.setdefault(letter, {})

        node['MATCH'] = (sequence, {
            'TARGET': target,
            'RESULT': {
                'BACKSPACES': -1,
                'FUNC': output_func,
                'OUTPUT': ""
            }
        })

    return trie


###############################################################################
def complete_sequence_trie(trie: Dict[str, Any], wordbreak_symbol: str) -> set[str]:
    outputs = set()

    def complete_node(sequence, action):
        nonlocal outputs

        back_sequence = []
        expanded_sequence = []

        for c in sequence[:-1]:
            back_sequence.append(c)
            expanded_sequence.append(c)
            match = get_trie_result(back_sequence)

            if not match:
                match = get_trie_result(expanded_sequence)

            if match:
                match_backspaces = match['RESULT']['BACKSPACES']
                match_output = match['RESULT']['OUTPUT']

                del expanded_sequence[-(match_backspaces + 1):]
                expanded_sequence.extend(match_output)
                # quiet_print(c, expanded_sequence)

        target = action['TARGET']

        i = 0
        while (
            i < min(len(expanded_sequence), len(target)) and
            expanded_sequence[i] == target[i]
        ):
            i += 1

        backspaces = len(expanded_sequence) - i
        output = target[i:].replace(wordbreak_symbol, " ")

        outputs.add(output)
        action['RESULT']['BACKSPACES'] = backspaces
        action['RESULT']['OUTPUT'] = output

    def get_trie_result(buffer: List[str]) -> dict[str, dict[str]]:
        longest_match = {}
        trienode = trie

        for c in reversed(buffer):
            if c not in trienode:
                break

            # quiet_print(c)
            trienode = trienode[c]

            if 'MATCH' in trienode:
                sequence, action = trienode['MATCH']

                if action['RESULT']['BACKSPACES'] == -1:
                    complete_node(sequence, action)

                longest_match = action

        return longest_match

    def traverse_trienode(trinode: Dict[str, Any]):
        for key, value in trinode.items():
            if key == 'MATCH':
                sequence, action = value

                if action['RESULT']['BACKSPACES'] == -1:
                    complete_node(sequence, action)
            else:
                traverse_trienode(value)

    traverse_trienode(trie)
    return outputs


###############################################################################
def generate_matches(pattern) -> list[tuple[str, str]]:
    valid_tokens = f"[\w{SEQ_TOKEN_SYMBOLS}{WORDBREAK_SYMBOL}]"

    square_brackets_group = re.findall(fr"\[({valid_tokens}+)]", pattern)
    if square_brackets_group:
        match = square_brackets_group[0]

        return generate_matches(pattern.replace(
            f"[{match}]",
            f"({'|'.join(match)})"
        ))

    patterns = []
    groups = re.findall(fr"\((?:{valid_tokens}\|?)+\)\??", pattern)

    if not groups:
        return [("", pattern)]

    match = groups[0]
    elements = re.findall(f"{valid_tokens}+", match)

    patterns.extend([
        (element, pattern.replace(match, element))
        for element in elements
    ])

    if "?" in match:
        patterns.append(("", pattern.replace(match, "")))

    return patterns


###############################################################################
def parse_tokens(tokens: List[str], parse_regex: bool) -> Iterator[Tuple[int, str, str]]:
    full_sequence, transform = tokens

    if parse_regex:
        for token, sequence in generate_matches(full_sequence):
            yield sequence, transform.replace(r"\1", token)
    else:
        yield full_sequence, transform


###############################################################################
def parse_file_lines(
    file_name: str, separator: str, comment: str
) -> Iterator[Tuple[int, str, str]]:
    """Parses lines read from `file_name` into sequence-transform pairs."""
    with open(file_name, 'rt', encoding="utf-8") as file:
        lines = file.readlines()

    regex_start = f"{COMMENT_STR}REGEX_START"
    regex_end = f"{COMMENT_STR}REGEX_END"
    in_regex_zone = False

    for line_number, line in enumerate(lines, 1):
        line = line.strip()

        if not line:
            continue

        if line == regex_start:
            in_regex_zone = True
            continue

        if line == regex_end:
            in_regex_zone = False
            continue

        if not line.startswith(comment):
            # Parse syntax "sequence -> transformation".
            # Using strip to ignore indenting.
            tokens = [token.strip() for token in line.split(separator, 1)]

            if len(tokens) != 2 or not tokens[0]:
                raise SystemExit(
                    f'{err(line_number)}: Invalid syntax: "{red(line)}"'
                )

            for sequence, transform in parse_tokens(tokens, in_regex_zone):
                yield line_number, sequence, transform


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
def serialize_sequence_trie(
    symbol_map: Dict[str, int], trie: Dict[str, Any],
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
            sequence, action = trie_node['MATCH']
            backspaces = action['RESULT']['BACKSPACES']
            max_backspaces = max(max_backspaces, backspaces)
            func = action['RESULT']['FUNC']
            output = action['RESULT']['OUTPUT']
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
            return data + [symbol_map[c] for c in node['chars']] + [0]

        else:  # Handle a branch table entry.
            links = []

            for c, link in zip(node['chars'], node['links']):
                links += [
                    symbol_map[c] | (0 if links else TRIE_BRANCH_BIT)
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
    symbol_map: Dict[str, int],
    sequence: str,
    transform: str
) -> Tuple[str, str]:
    """ returns a pair of strings filled with C definitions
        transform gets cleaned up but stays a string (for now),
        sequence gets converted to array of uint16_t
        ex: "(uint16_t[4]){ 0x1234, 0x1234, 0x1234, 0},"
    """
    # we don't want any utf8 symbols in transformation string here
    transform_dict = {
        "\\": "\\\\",
        WORDBREAK_SYMBOL: " ",
        **{sym: char for sym, char in zip(SEQ_TOKEN_SYMBOLS, SEQ_TOKEN_ASCII_CHARS)}
    }
    for (i, j) in transform_dict.items():
        transform = transform.replace(i, j)
    trans_c_str = f'    "{transform}",'
    seq_ints = [symbol_map[c] for c in sequence] + [0]
    seq_int_str = ', '.join(map(uint16_to_hex, seq_ints))
    seq_c_str   = f'    (uint16_t[{len(seq_ints)}]){{ {seq_int_str} }},'
    return seq_c_str, trans_c_str


###############################################################################
def generate_sequence_transform_data(data_header_file, test_header_file):
    symbol_map = generate_sequence_symbol_map(SEQ_TOKEN_SYMBOLS, WORDBREAK_SYMBOL)
    output_func_symbol_map = generate_output_func_symbol_map(OUTPUT_FUNC_SYMBOLS)

    seq_tranform_list = parse_file(RULES_FILE, symbol_map, SEP_STR, COMMENT_STR)
    trie = make_sequence_trie(seq_tranform_list, output_func_symbol_map)
    outputs = complete_sequence_trie(trie, WORDBREAK_SYMBOL)
    quiet_print(json.dumps(trie, indent=4))

    s_outputs = serialize_outputs(outputs)
    completions_data, completions_map, max_completion_len = s_outputs

    trie_data = serialize_sequence_trie(symbol_map, trie, completions_map)

    assert all(0 <= b <= 0xffff for b in trie_data)
    assert all(0 <= b <= 0xff for b in completions_data)

    min_sequence = min(seq_tranform_list, key=sequence_len)[0]
    max_sequence = max(seq_tranform_list, key=sequence_len)[0]
    max_transform = max(seq_tranform_list, key=transform_len)[1]

    # Build the sequence_transform_data.h file.
    transforms = []
    test_rule_c_sequences = []
    test_rule_c_transforms = []

    for sequence, transform in seq_tranform_list:
        # Don't add rules with transformation functions to test header for now
        if transform[-1] not in output_func_symbol_map:
            transform_without_leading_wordbreak = transform[1:] if sequence.startswith(WORDBREAK_SYMBOL) else transform
            test_rule = create_test_rule_c_string(symbol_map, sequence, transform_without_leading_wordbreak)
            test_rule_c_sequences.append(test_rule[0])
            test_rule_c_transforms.append(test_rule[1])
        transform = transform.replace("\\", "\\ [escape]")
        sequence = f"{sequence:<{len(max_sequence)}}"
        transforms.append(f'//    {sequence} -> {transform}')

    header_lines = [
        GPL2_HEADER_C_LIKE,
        GENERATED_HEADER_C_LIKE,
        '#pragma once',
    ]

    tranforms_lines = [
        f'// Sequence Transformation dictionary with longest match semantics',
        f'// Dictionary ({len(seq_tranform_list)} entries):',
        *transforms,
    ]

    # token symbols stored as utf8 strings
    sym_array_str = ", ".join(map(lambda c: f'"{c}"', SEQ_TOKEN_SYMBOLS))
    st_seq_tokens = f'static const char *st_seq_tokens[] = {{ {sym_array_str} }};'
    st_wordbreak_token = f'static const char *st_wordbreak_token = "{WORDBREAK_SYMBOL}";'
    # ascii versions
    char_array_str = ", ".join(map(lambda c: f"'{c}'", SEQ_TOKEN_ASCII_CHARS))
    st_seq_token_ascii_chars = f'static const char st_seq_token_ascii_chars[] = {{ {char_array_str} }};'
    st_wordbreak_ascii = f"static const char st_wordbreak_ascii = '{WORDBREAK_ASCII}';"

    trie_stats_lines = [
        f'#define {ST_GENERATOR_VERSION}',
        '',
        f'#define TRIECODE_SEQUENCE_TOKEN_0 {uint16_to_hex(TRIECODE_SEQUENCE_TOKEN_0)}',
        f'#define SEQUENCE_MIN_LENGTH {len(min_sequence)} // "{min_sequence}"',
        f'#define SEQUENCE_MAX_LENGTH {len(max_sequence)} // "{max_sequence}"',
        f'#define TRANSFORM_MAX_LENGTH {len(max_transform)} // "{max_transform}"',
        f'#define COMPLETION_MAX_LENGTH {max_completion_len}',
        f'#define MAX_BACKSPACES {max_backspaces}',
        f'#define SEQUENCE_TRIE_SIZE {len(trie_data)}',
        f'#define COMPLETIONS_SIZE {len(completions_data)}',
        f'#define SEQUENCE_TOKEN_COUNT {len(SEQ_TOKEN_SYMBOLS)}',
        '',
        st_seq_token_ascii_chars,
        st_wordbreak_ascii
    ]

    trie_data_lines = [
        'static const uint16_t '
        'sequence_transform_data[SEQUENCE_TRIE_SIZE] PROGMEM = {',

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
        *tranforms_lines,
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
        'static const uint16_t *st_test_sequences[] = {',
        *test_rule_c_sequences,
        '    0',
        '};',
        '',
        'static const char *st_test_transforms[] = {',
        *test_rule_c_transforms,
        '    0',
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
        SEQ_TOKEN_ASCII_CHARS = config['sequence_token_ascii_chars']
        WORDBREAK_ASCII = config['wordbreak_ascii']
        SEQ_TOKEN_SYMBOLS = config['sequence_token_symbols']
        OUTPUT_FUNC_SYMBOLS = config['output_func_symbols']
        WORDBREAK_SYMBOL = config['wordbreak_symbol']
        COMMENT_STR = config['comment_str']
        SEP_STR = config['separator_str']
        RULES_FILE = THIS_FOLDER / "../../" / config['rules_file_name']
    except KeyError as e:
        raise SystemExit(f"Incorrect config! {cyan(*e.args)} key is missing.")

    IS_QUIET = config.get("quiet", True)
    IMPLICIT_TRANSFORM_LEADING_WORDBREAK = config.get('implicit_transform_leading_wordbreak', False)

    if cli_args.quiet:
        IS_QUIET = True

    generate_sequence_transform_data(data_header_file, test_header_file)
