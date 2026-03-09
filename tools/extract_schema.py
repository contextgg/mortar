#!/usr/bin/env python3
"""
extract_schema.py — Parse C++ ECS headers and generate schema.json + schema.d.ts.

Reads struct/enum definitions from components.h and spawner.h,
merges metadata from schema_meta.toml, and outputs the schema files.

Usage:
    python tools/extract_schema.py [--out-dir DIR] [--commit SHA]
"""

import argparse
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:
    import tomli as tomllib  # Python < 3.11


# ── C++ type mapping ──────────────────────────────────────────────────────────

CPP_TYPE_MAP = {
    "float": "float",
    "int": "int",
    "uint32_t": "int",
    "bool": "bool",
    "glm::vec2": "vec2",
    "glm::vec3": "vec3",
    "glm::vec4": "vec4",
    "glm::quat": "quat",
}

# ── Regex patterns ────────────────────────────────────────────────────────────

RE_STRUCT_START = re.compile(r"^struct\s+(\w+)\s*\{", re.MULTILINE)
RE_ENUM_CLASS = re.compile(
    r"enum\s+class\s+(\w+)\s*\{([^}]*)\}", re.MULTILINE | re.DOTALL
)

# Field patterns:
#   glm::vec3 field{x, y, z};
#   glm::quat field{w, x, y, z};
#   float field = 1.0f;
#   int field = 5;
#   bool field = true;
#   EnumType field = EnumType::Value;
#   uint32_t field = 0;
RE_FIELD_BRACE = re.compile(
    r"^\s*([\w:]+)\s+(\w+)\s*\{([^}]*)\}\s*;", re.MULTILINE
)
RE_FIELD_ASSIGN = re.compile(
    r"^\s*([\w:]+)\s+(\w+)\s*=\s*(.+?)\s*;", re.MULTILINE
)
RE_FIELD_BARE = re.compile(
    r"^\s*([\w:]+)\s+(\w+)\s*;", re.MULTILINE
)


def parse_brace_default(cpp_type: str, raw: str):
    """Parse a brace-initialised default like {0.0f, 1.0f, 0.0f}.

    Handles GLM broadcast: glm::vec3{0.0f} → [0, 0, 0]
    """
    parts = [p.strip().rstrip("f") for p in raw.split(",")]
    length_map = {"glm::vec2": 2, "glm::vec3": 3, "glm::vec4": 4, "glm::quat": 4}

    if cpp_type in length_map:
        expected = length_map[cpp_type]
        vals = [float(p) for p in parts if p]
        # GLM broadcast: single value fills all components
        if len(vals) == 1:
            vals = vals * expected
        if cpp_type == "glm::quat":
            # glm::quat{w, x, y, z} in C++ → JSON stores [x, y, z, w]
            return [vals[1], vals[2], vals[3], vals[0]]
        return vals[:expected]

    if len(parts) == 1:
        return parse_scalar(parts[0])
    return [float(p) for p in parts]


def parse_scalar(raw: str):
    """Parse a scalar default value."""
    raw = raw.strip().rstrip("f")
    if raw in ("true",):
        return True
    if raw in ("false",):
        return False
    # Enum: EnumType::Value → snake_case lowercase
    if "::" in raw:
        return to_snake_case(raw.split("::")[-1]).lower()
    try:
        if "." in raw:
            return float(raw)
        return int(raw)
    except ValueError:
        return raw


def to_snake_case(name: str) -> str:
    """Convert CamelCase to snake_case."""
    s1 = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", name)
    s2 = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s1)
    return s2.lower()


# ── Header parsing ────────────────────────────────────────────────────────────

def parse_enums(text: str) -> dict:
    """Extract enum class definitions."""
    enums = {}
    for m in RE_ENUM_CLASS.finditer(text):
        name = m.group(1)
        body = m.group(2)
        values = []
        for val in body.split(","):
            val = val.strip()
            if val:
                values.append(to_snake_case(val).lower())
        if values:
            enums[name] = {"values": values, "default": values[0]}
    return enums


def extract_struct_body(text: str, start: int) -> str:
    """Extract the body of a struct using brace-counting from the opening {."""
    depth = 0
    body_start = None
    for i in range(start, len(text)):
        if text[i] == "{":
            if depth == 0:
                body_start = i + 1
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[body_start:i]
    return ""


def parse_structs(text: str) -> dict:
    """Extract struct definitions with fields, types, and defaults."""
    structs = {}
    for m in RE_STRUCT_START.finditer(text):
        struct_name = m.group(1)
        body = extract_struct_body(text, m.start())

        fields = {}
        seen_fields = set()

        # Pass 1: brace-initialised fields
        for fm in RE_FIELD_BRACE.finditer(body):
            cpp_type, field_name, raw_default = fm.group(1), fm.group(2), fm.group(3)
            if field_name in seen_fields:
                continue
            seen_fields.add(field_name)
            schema_type = CPP_TYPE_MAP.get(cpp_type, cpp_type)
            default = parse_brace_default(cpp_type, raw_default)
            fields[field_name] = {"type": schema_type, "default": default}

        # Pass 2: assignment-initialised fields
        for fm in RE_FIELD_ASSIGN.finditer(body):
            cpp_type, field_name, raw_default = fm.group(1), fm.group(2), fm.group(3)
            if field_name in seen_fields:
                continue
            seen_fields.add(field_name)
            schema_type = CPP_TYPE_MAP.get(cpp_type, cpp_type)
            default = parse_scalar(raw_default)
            # If type is an enum, resolve
            if "::" in fm.group(3):
                enum_name = fm.group(3).split("::")[0].strip()
                schema_type = enum_name
            fields[field_name] = {"type": schema_type, "default": default}

        # Pass 3: bare fields (no default)
        for fm in RE_FIELD_BARE.finditer(body):
            cpp_type, field_name = fm.group(1), fm.group(2)
            if field_name in seen_fields:
                continue
            seen_fields.add(field_name)
            schema_type = CPP_TYPE_MAP.get(cpp_type, cpp_type)
            fields[field_name] = {"type": schema_type}

        structs[struct_name] = fields

    return structs


# ── Schema assembly ───────────────────────────────────────────────────────────

def build_schema(
    header_files: list[Path], meta_path: Path, commit: str = "unknown"
) -> dict:
    """Build the full schema dict from headers + metadata."""
    # Read and concatenate headers
    all_text = ""
    for hf in header_files:
        all_text += hf.read_text() + "\n"

    # Parse C++ structures
    raw_enums = parse_enums(all_text)
    raw_structs = parse_structs(all_text)

    # Read metadata
    with open(meta_path, "rb") as f:
        meta = tomllib.load(f)

    runtime_only = set(meta.get("runtime_only_components", {}).get("list", []))

    # Built-in types
    types = {
        "vec2": {"kind": "array", "element": "float", "length": 2},
        "vec3": {"kind": "array", "element": "float", "length": 3},
        "vec4": {"kind": "array", "element": "float", "length": 4},
        "quat": {"kind": "array", "element": "float", "length": 4},
        "rgba": {"kind": "array", "element": "float", "length": 4},
    }

    # Enums from C++ + synthetic
    enums = {}
    for name, data in raw_enums.items():
        enums[name] = data
    for name, data in meta.get("synthetic_enums", {}).items():
        enums[name] = dict(data)

    # Components from C++ (skip runtime-only)
    components = {}
    comp_meta = meta.get("components", {})
    for struct_name, fields in raw_structs.items():
        if struct_name in runtime_only:
            continue
        # Empty structs (tags) are handled separately
        if not fields:
            continue

        snake = to_snake_case(struct_name)
        cm = comp_meta.get(snake, {})

        comp = {}
        if "display_name" in cm:
            comp["display_name"] = cm["display_name"]
        else:
            comp["display_name"] = struct_name
        if "category" in cm:
            comp["category"] = cm["category"]
        if cm.get("required"):
            comp["required"] = True

        field_meta = cm.get("fields", {})
        comp_fields = {}
        for fname, fdata in fields.items():
            fd = dict(fdata)
            fm = field_meta.get(fname, {})
            if fm.get("runtime_only"):
                fd["runtime_only"] = True
            if "ui" in fm:
                fd["ui"] = dict(fm["ui"])
            if "condition" in fm:
                fd["condition"] = fm["condition"]
            comp_fields[fname] = fd
        comp["fields"] = comp_fields
        components[snake] = comp

    # Synthetic components from meta
    for name, data in meta.get("synthetic_components", {}).items():
        comp = {}
        if "display_name" in data:
            comp["display_name"] = data["display_name"]
        if "category" in data:
            comp["category"] = data["category"]
        syn_fields = {}
        for fname, fdata in data.get("fields", {}).items():
            fd = {}
            fd["type"] = fdata["type"]
            if "default" in fdata:
                fd["default"] = fdata["default"]
            if "ui" in fdata:
                fd["ui"] = dict(fdata["ui"])
            if "condition" in fdata:
                fd["condition"] = fdata["condition"]
            syn_fields[fname] = fd
        comp["fields"] = syn_fields
        components[name] = comp

    # Tags (empty structs that are in the tags meta)
    tags = {}
    for name, data in meta.get("tags", {}).items():
        tags[name] = dict(data)

    # Entity types
    entity_types = {}
    for name, data in meta.get("entity_types", {}).items():
        et = {"display_name": data["display_name"]}
        et["default_components"] = list(data["default_components"])
        if "default_mesh" in data:
            et["default_mesh"] = data["default_mesh"]
        entity_types[name] = et

    # Map settings
    map_settings = {}
    for name, data in meta.get("map_settings", {}).items():
        ms = {"type": data["type"]}
        if "default" in data:
            ms["default"] = data["default"]
        map_settings[name] = ms

    return {
        "schema_version": "0.0.0",  # placeholder — bump_version.py sets this
        "engine_commit": commit,
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "types": types,
        "enums": enums,
        "components": components,
        "tags": tags,
        "entity_types": entity_types,
        "map_settings": map_settings,
    }


# ── TypeScript type generation ────────────────────────────────────────────────

def generate_dts(schema: dict) -> str:
    """Generate schema.d.ts from the schema dict."""
    lines = [
        "// Auto-generated by extract_schema.py — do not edit manually",
        "",
    ]

    # Type aliases
    for name, td in schema["types"].items():
        ts_elem = "number" if td["element"] == "float" else td["element"]
        lines.append(
            f"export type {name.capitalize()} = [{', '.join([ts_elem] * td['length'])}];"
        )
    lines.append("")

    # Enums as union types
    for name, ed in schema["enums"].items():
        values = " | ".join(f"'{v}'" for v in ed["values"])
        lines.append(f"export type {name} = {values};")
    lines.append("")

    # Entity type union
    et_values = " | ".join(f"'{k}'" for k in schema["entity_types"])
    lines.append(f"export type EntityType = {et_values};")
    lines.append("")

    # Component interfaces
    ts_type_map = {
        "float": "number",
        "int": "number",
        "bool": "boolean",
        "vec2": "Vec2",
        "vec3": "Vec3",
        "vec4": "Vec4",
        "quat": "Quat",
        "rgba": "Rgba",
    }

    for comp_name, comp in schema["components"].items():
        iface_name = "".join(w.capitalize() for w in comp_name.split("_")) + "Data"
        lines.append(f"export interface {iface_name} {{")
        for fname, fdata in comp["fields"].items():
            if fdata.get("runtime_only"):
                continue
            ftype = fdata["type"]
            ts_type = ts_type_map.get(ftype)
            if ts_type is None:
                # Check if it's an enum
                if ftype in schema["enums"]:
                    ts_type = ftype
                else:
                    ts_type = "unknown"
            optional = "?" if fdata.get("condition") else ""
            lines.append(f"  {fname}{optional}: {ts_type};")
        lines.append("}")
        lines.append("")

    # MapSettings
    lines.append("export interface MapSettings {")
    for name, data in schema["map_settings"].items():
        ftype = data["type"]
        ts_type = ts_type_map.get(ftype, "unknown")
        lines.append(f"  {name}: {ts_type};")
    lines.append("}")
    lines.append("")

    # MapEntity
    lines.append("export interface MapEntity {")
    lines.append("  id: string;")
    lines.append("  name: string;")
    lines.append("  type: EntityType;")
    lines.append("  mesh?: string;")
    for comp_name, comp in schema["components"].items():
        iface_name = "".join(w.capitalize() for w in comp_name.split("_")) + "Data"
        required = comp.get("required", False)
        opt = "" if required else "?"
        lines.append(f"  {comp_name}{opt}: {iface_name};")
    lines.append("  tags?: string[];")
    lines.append("}")
    lines.append("")

    # MortarMap
    lines.append("export interface MortarMap {")
    lines.append("  version: number;")
    lines.append("  name: string;")
    lines.append("  created_at: string;")
    lines.append("  modified_at: string;")
    lines.append("  settings: MapSettings;")
    lines.append("  entities: MapEntity[];")
    lines.append("}")
    lines.append("")

    return "\n".join(lines)


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Extract schema from C++ headers")
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path("."),
        help="Directory to write schema.json and schema.d.ts",
    )
    parser.add_argument(
        "--commit",
        default="unknown",
        help="Engine commit SHA to embed in schema",
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=None,
        help="Root of the mortar repo (defaults to parent of tools/)",
    )
    args = parser.parse_args()

    repo_root = args.repo_root or Path(__file__).resolve().parent.parent
    headers = [
        repo_root / "src" / "ecs" / "components.h",
        repo_root / "src" / "game" / "spawner.h",
    ]
    meta_path = repo_root / "tools" / "schema_meta.toml"

    for p in [*headers, meta_path]:
        if not p.exists():
            print(f"Error: {p} not found", file=sys.stderr)
            sys.exit(1)

    schema = build_schema(headers, meta_path, args.commit)

    out_dir = args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    schema_path = out_dir / "schema.json"
    schema_path.write_text(json.dumps(schema, indent=2) + "\n")
    print(f"Wrote {schema_path}")

    dts_path = out_dir / "schema.d.ts"
    dts_path.write_text(generate_dts(schema))
    print(f"Wrote {dts_path}")


if __name__ == "__main__":
    main()
