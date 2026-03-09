#!/usr/bin/env python3
"""
bump_version.py — Determine semver bump by diffing old and new schema.json.

Rules:
  - Major: component or field removed (breaking change)
  - Minor: component or field added
  - Patch: only defaults, metadata, or ui hints changed

Usage:
    python tools/bump_version.py --old old/schema.json --new new/schema.json
    python tools/bump_version.py --new new/schema.json  # first release → 1.0.0

Outputs the new version string to stdout (e.g. "1.2.0").
Also patches schema_version in the --new file in-place.
"""

import argparse
import json
import sys
from pathlib import Path


def parse_version(v: str) -> tuple[int, int, int]:
    parts = v.split(".")
    return int(parts[0]), int(parts[1]), int(parts[2])


def bump(major: int, minor: int, patch: int, level: str) -> str:
    if level == "major":
        return f"{major + 1}.0.0"
    if level == "minor":
        return f"{major}.{minor + 1}.0"
    return f"{major}.{minor}.{patch + 1}"


def diff_schemas(old: dict, new: dict) -> str:
    """Compare two schemas and return the bump level needed."""
    bump_level = "patch"  # assume patch unless we find bigger changes

    old_comps = set(old.get("components", {}).keys())
    new_comps = set(new.get("components", {}).keys())

    # Removed components → major
    removed_comps = old_comps - new_comps
    if removed_comps:
        print(f"MAJOR: removed components: {removed_comps}", file=sys.stderr)
        return "major"

    # Added components → minor
    added_comps = new_comps - old_comps
    if added_comps:
        print(f"MINOR: added components: {added_comps}", file=sys.stderr)
        bump_level = "minor"

    # Check fields within shared components
    for comp_name in old_comps & new_comps:
        old_fields = set(old["components"][comp_name].get("fields", {}).keys())
        new_fields = set(new["components"][comp_name].get("fields", {}).keys())

        removed_fields = old_fields - new_fields
        if removed_fields:
            print(
                f"MAJOR: {comp_name} removed fields: {removed_fields}",
                file=sys.stderr,
            )
            return "major"

        added_fields = new_fields - old_fields
        if added_fields:
            print(
                f"MINOR: {comp_name} added fields: {added_fields}", file=sys.stderr
            )
            bump_level = "minor"

    # Check enums
    old_enums = set(old.get("enums", {}).keys())
    new_enums = set(new.get("enums", {}).keys())
    if old_enums - new_enums:
        print(f"MAJOR: removed enums: {old_enums - new_enums}", file=sys.stderr)
        return "major"
    if new_enums - old_enums:
        print(f"MINOR: added enums: {new_enums - old_enums}", file=sys.stderr)
        bump_level = "minor"

    # Check enum values removed
    for enum_name in old_enums & new_enums:
        old_vals = set(old["enums"][enum_name].get("values", []))
        new_vals = set(new["enums"][enum_name].get("values", []))
        if old_vals - new_vals:
            print(
                f"MAJOR: {enum_name} removed values: {old_vals - new_vals}",
                file=sys.stderr,
            )
            return "major"
        if new_vals - old_vals:
            print(
                f"MINOR: {enum_name} added values: {new_vals - old_vals}",
                file=sys.stderr,
            )
            bump_level = "minor"

    # Check entity types
    old_et = set(old.get("entity_types", {}).keys())
    new_et = set(new.get("entity_types", {}).keys())
    if old_et - new_et:
        print(f"MAJOR: removed entity_types: {old_et - new_et}", file=sys.stderr)
        return "major"
    if new_et - old_et:
        print(f"MINOR: added entity_types: {new_et - old_et}", file=sys.stderr)
        bump_level = "minor"

    return bump_level


def main():
    parser = argparse.ArgumentParser(description="Determine semver bump for schema")
    parser.add_argument("--old", type=Path, help="Path to previous schema.json")
    parser.add_argument("--new", type=Path, required=True, help="Path to new schema.json")
    args = parser.parse_args()

    new_schema = json.loads(args.new.read_text())

    if args.old is None or not args.old.exists():
        # First release
        version = "1.0.0"
        print("First release → 1.0.0", file=sys.stderr)
    else:
        old_schema = json.loads(args.old.read_text())
        old_version = old_schema.get("schema_version", "0.0.0")
        major, minor, patch = parse_version(old_version)
        level = diff_schemas(old_schema, new_schema)
        version = bump(major, minor, patch, level)
        print(f"{old_version} → {version} ({level})", file=sys.stderr)

    # Patch the new schema file in-place
    new_schema["schema_version"] = version
    args.new.write_text(json.dumps(new_schema, indent=2) + "\n")

    # Output just the version to stdout for CI scripts
    print(version)


if __name__ == "__main__":
    main()
