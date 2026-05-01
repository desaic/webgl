r"""Strip a CAD hierarchy JSON down to the keys used by the TVG matcher.

Removes geometry metadata (JT TriStripper LOD, SUBNODE markers, CAD mass/
volume/density, REFSETs, etc.) and prunes subtrees that carry no assembly-
structure information. Typical reduction is ~94% on a full BMW hierarchy
export (e.g. 176 MB -> 11 MB).

Kept keys (the only ones ``match_tvg_parts_to_glb.load_cad_hierarchy``
reads):
  - ``children``
  - ``JT_PROP_NAME``
  - ``ud_PDM_INST_PPG::``
  - ``ud_PDM_NAME_GER::``

Usage:
    pixi run python -m modules_service.debug.slim_cad_hierarchy \\
        --input path/to/hierarchy.json \\
        --output path/to/hierarchy_slim.json
"""

import argparse
import json
import logging
import re
import sys
from pathlib import Path

logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
logger = logging.getLogger(__name__)

_USEFUL_KEYS = {"JT_PROP_NAME", "ud_PDM_INST_PPG::", "ud_PDM_NAME_GER::"}
_KEPT_KEYS = _USEFUL_KEYS | {"children"}


def _has_useful_data(node: dict) -> bool:
    if any(k in node for k in _USEFUL_KEYS):
        return True
    return any(_has_useful_data(c) for c in node.get("children", []))


def _prune(node: dict) -> dict:
    pruned = {k: node[k] for k in _KEPT_KEYS if k in node}
    children = node.get("children", [])
    if children:
        pruned_children = [
            _prune(c) for c in children if _has_useful_data(c)
        ]
        if pruned_children:
            pruned["children"] = pruned_children
        else:
            pruned.pop("children", None)
    return pruned


def _count_nodes(node: dict) -> int:
    return 1 + sum(_count_nodes(c) for c in node.get("children", []))


def run(input_path: Path, output_path: Path) -> None:
    logger.info("Loading %s", input_path)
    with input_path.open(encoding="utf-8") as f:
            # 1. Load the entire file into a string
        content = f.read()
        
        # 2. Clean the specific character (char 218 is \u00da)
        # This takes ~0.1 seconds for 150MB
            # This specifically targets the "invisible" characters that break JSON
        clean_content = re.sub(r'\t', ' ', content)
        # 3. Parse the cleaned string
        root = json.loads(clean_content)

    orig_nodes = _count_nodes(root)
    pruned = _prune(root)
    new_nodes = _count_nodes(pruned)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8") as f:
        json.dump(pruned, f, separators=(",", ":"))

    orig_size = input_path.stat().st_size
    new_size = output_path.stat().st_size
    logger.info(
        "Original: %.1f MB, %d nodes", orig_size / 1e6, orig_nodes
    )
    logger.info(
        "Pruned:   %.1f MB, %d nodes (%.1f%% reduction)",
        new_size / 1e6,
        new_nodes,
        (1 - new_size / orig_size) * 100,
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Strip a CAD hierarchy JSON to the keys used by the TVG matcher."
    )
    parser.add_argument(
        "--input", type=Path, required=True, help="Input hierarchy JSON"
    )
    parser.add_argument(
        "--output", type=Path, required=True, help="Output slim JSON"
    )
    args = parser.parse_args()
    if not args.input.exists():
        logger.error("Input file not found: %s", args.input)
        sys.exit(1)
    run(args.input, args.output)


if __name__ == "__main__":
    main()