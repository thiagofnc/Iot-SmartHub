"""
Convert SmartHub dataset capture into Edge Impulse "Infer from filename" format.

Input layout (what the camera firmware writes):
    <src>/up/up_<timestamp>_001.jpg ... _NNN.jpg
    <src>/down/down_<timestamp>_001.jpg ...
    <src>/lswipe/...
    <src>/rswipe/...
    <src>/rotate/...
    <src>/idle/...

Output layout (flat, ready for Edge Impulse web uploader):
    <dst>/up.<unique>.jpg
    <dst>/down.<unique>.jpg
    ...

In the Edge Impulse uploader, set "Label" to "Infer from filename".

Usage:
    python prepare_for_edge_impulse.py <src_dataset_dir> <dst_dir>

Example:
    python prepare_for_edge_impulse.py D:/dataset ./dataset_for_ei
"""

import shutil
import sys
from pathlib import Path

LABELS = ["up", "down", "lswipe", "rswipe", "rotate", "idle"]


def main(src: Path, dst: Path) -> int:
    if not src.is_dir():
        print(f"ERROR: source directory not found: {src}", file=sys.stderr)
        return 1

    dst.mkdir(parents=True, exist_ok=True)

    total = 0
    skipped = 0
    per_label: dict[str, int] = {label: 0 for label in LABELS}

    for label in LABELS:
        label_dir = src / label
        if not label_dir.is_dir():
            print(f"  (skipping {label}: directory missing)")
            continue

        for jpg in sorted(label_dir.iterdir()):
            if not jpg.is_file():
                skipped += 1
                continue
            # The XIAO's FATFS truncates LFN at 14 chars, so files end up as
            # "up_6265_001.jp" instead of ".jpg". Accept any "j*" extension
            # since the byte contents are still JPEG.
            ext = jpg.suffix.lower()
            if not ext.startswith(".j"):
                skipped += 1
                continue

            # Source filename is e.g. "up_330138_007.jpg".
            # We want a unique id that preserves clip+frame ordering so
            # collisions are impossible across all clips for this label.
            stem = jpg.stem
            if stem.startswith(label + "_"):
                unique = stem[len(label) + 1:]
            else:
                unique = stem

            unique = unique.replace("_", "-")
            new_name = f"{label}.{unique}.jpg"
            shutil.copyfile(jpg, dst / new_name)
            per_label[label] += 1
            total += 1

    print(f"\nCopied {total} files to {dst}")
    print(f"Skipped (non-jpg): {skipped}")
    for label in LABELS:
        print(f"  {label:8s}: {per_label[label]}")
    print("\nNow upload the contents of the destination folder to Edge Impulse.")
    print("In the uploader: set Label to 'Infer from filename', upload category 'Training',")
    print("and enable 'Automatically split into training and testing'.")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python prepare_for_edge_impulse.py <src_dataset_dir> <dst_dir>")
        sys.exit(2)
    sys.exit(main(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()))
