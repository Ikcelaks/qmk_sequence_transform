from argparse import ArgumentParser
import json
from pathlib import Path


if __name__ == '__main__':
    parser = ArgumentParser()

    parser.add_argument(
        "-c", "--config", type=str,
        help="config file", default="sequence_transform_config.json"
    )

    parser.add_argument("-d", "--debug", action="store_true", default=False)
    parser.add_argument("-p", "--path", type=Path)
    cli_args = parser.parse_args()

    THIS_FOLDER = Path(__file__).parent

    data_header_file = THIS_FOLDER / "../sequence_transform_data.h"
    metadata_header_file = THIS_FOLDER / "../st_gen_metadata.h"
    test_header_file = THIS_FOLDER / "../sequence_transform_test.h"
    default_config_file = THIS_FOLDER / "sequence_transform_config_default.json"
    keymap_st_path = cli_args.path / "sequence_transform"
    user_config_file = keymap_st_path / cli_args.config
    Path.mkdir(keymap_st_path, exist_ok=True)
    config = json.load(open(default_config_file, 'rt', encoding="utf-8"))
    if user_config_file.is_file():
        user_config = json.load(open(user_config_file, 'rt', encoding="utf-8"))
        config.update(user_config)
    else:
        with open(file=user_config_file, mode="a") as cf:
            json.dump({}, cf)
