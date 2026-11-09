from argparse import ArgumentParser
from dataclasses import dataclass
import json
from pathlib import Path
from typing import Any

@dataclass
class SequenceTransformConfig:
    sequence_token_symbols: dict[str, str]
    space_symbol: str
    wordbreak_symbol: str
    digit_symbol: str
    alpha_symbol: str
    upper_alpha_symbol: str
    punct_symbol: str
    nonterminating_punct_symbol: str
    terminating_punct_symbol: str
    any_symbol: str
    output_func_one_shot_shift_symbol: str
    transform_sequence_reference_symbols: list[str]
    comment_str: str
    separator_str: str

    def update(self, other: dict[str, Any]):
        for key, value in other.items():
            if hasattr(self, key):
                setattr(self, key, value)

    @staticmethod
    def load_config(config_file: Path) -> Self:
        with open(config_file, 'rt', encoding="utf-8") as cf:
            config_data = json.load(cf)
        return SequenceTransformConfig(**config_data)

###############################################################################
def create_rules_dict_template_if_missing(
    file_name: Path
):
    if not file_name.is_file():
        with open(file_name, mode="a", encoding="utf-8") as rf:
            rf.write(f'{COMMENT_STR} Sequence Transform Rule File: {file_name.name}\n')
            rf.write(f'{COMMENT_STR}  Sequence Tokens:\n')
            for st in SEQ_TOKEN_SYMBOLS:
                rf.write(f'{COMMENT_STR}   {st}\n')
            rf.write(f'{COMMENT_STR}  Transform Sequence Reference Symbols:\n')
            for tsrs in TRANSFORM_SEQUENCE_REFERENCE_SYMBOLS:
                rf.write(f'{COMMENT_STR}   {tsrs}\n')
            rf.write(f'{COMMENT_STR}  Space Symbol: {SPACE_SYMBOL}\n')
            rf.write(f'{COMMENT_STR}  Wordbreak MetaCharacter Symbol: {WORDBREAK_SYMBOL}\n')
            rf.write(f'{COMMENT_STR}  Digit MetaCharacter Symbol: {DIGIT_SYMBOL}\n')
            rf.write(f'{COMMENT_STR}  Alpha MetaCharacter Symbol: {ALPHA_SYMBOL}\n')
            rf.write(f'{COMMENT_STR}  Upper-Alpha MetaCharacter Symbol: {UPPER_ALPHA_SYMBOL}\n')
            rf.write(f'{COMMENT_STR}  Terminating Punctuation MetaCharacter Symbol: {TERMINATING_PUNCT_SYMBOL}\n')
            rf.write(f'{COMMENT_STR}  Non-terminating Punctuation MetaCharacter Symbol: {NONTERMINATING_PUNCT_SYMBOL}\n')
            rf.write(f'{COMMENT_STR}  Punctuation MetaCharacter Symbol: {PUNCT_SYMBOL}\n')
            rf.write(f'{COMMENT_STR}  Any MetaCharacter Symbol: {ANY_SYMBOL}\n')
            rf.write(f'{COMMENT_STR}  Output Func OneShot Shift Symbol: {ONE_SHOT_SHIFT_SYMBOL}\n')
            rf.write(f'{COMMENT_STR}  Separator String: {SEP_STR}\n')

###############################################################################
def get_symbol_config(config: Any, key: str) -> str:
    item = config[key]
    return ''.join(item.keys()) if isinstance(item, dict) else item

if __name__ == '__main__':
    parser = ArgumentParser()

    parser.add_argument(
        "-c", "--config-path", type=Path,
        help="config file", required=True
    )

    cli_args = parser.parse_args()

    THIS_FOLDER = Path(__file__).parent

    default_config_file = THIS_FOLDER / "sequence_transform_config_default.json"
    user_config_path = cli_args.config_path # type: Path
    user_config_file = user_config_path / "config.json"
    user_rules_priority_file = user_config_path / "rules_priority"
    Path.mkdir(user_config_path, exist_ok=True)
    config = load_config(default_config_file)

    if not user_config_file.is_file():
        with open(file=user_config_file, mode="a") as cf:
            json.dump(config, cf, ensure_ascii=False, indent=4)
    else:
        with open(user_config_file, 'rt', encoding="utf-8") as ucf:
            user_config = json.load(ucf)
        config.update(user_config)

    SEQ_TOKEN_SYMBOLS = list(config['sequence_token_symbols'].keys())
    SPACE_SYMBOL = get_symbol_config(config, 'space_symbol')
    WORDBREAK_SYMBOL = get_symbol_config(config, 'wordbreak_symbol')
    DIGIT_SYMBOL = get_symbol_config(config, 'digit_symbol')
    ALPHA_SYMBOL = get_symbol_config(config, 'alpha_symbol')
    UPPER_ALPHA_SYMBOL = get_symbol_config(config, 'upper_alpha_symbol')
    PUNCT_SYMBOL = get_symbol_config(config, 'punct_symbol')
    NONTERMINATING_PUNCT_SYMBOL = get_symbol_config(config, 'nonterminating_punct_symbol')
    TERMINATING_PUNCT_SYMBOL = get_symbol_config(config, 'terminating_punct_symbol')
    ANY_SYMBOL = get_symbol_config(config, 'any_symbol')
    ONE_SHOT_SHIFT_SYMBOL = config['output_func_one_shot_shift_symbol']
    TRANSFORM_SEQUENCE_REFERENCE_SYMBOLS = config['transform_sequence_reference_symbols']
    COMMENT_STR = config['comment_str']
    SEP_STR = config['separator_str']

    user_rules_files = []
    if not user_rules_priority_file.is_file():
        user_rules_files = [user_config_path/ 'sample.rules']
        with open(file=user_rules_priority_file, mode="a") as rpf:
            rpf.write("sample.rules")
    else:
        with open(file=user_rules_priority_file) as rpf:
            user_rules_files = [ user_config_path / rules_file.strip() for rules_file in rpf]

    for rules_file in user_rules_files:
        create_rules_dict_template_if_missing(rules_file)

    print(f'{user_config_file} {" ".join([str(rf) for rf in user_rules_files])}')
