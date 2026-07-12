#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import re
import shutil
import stat
import subprocess
import sys
from pathlib import Path
from typing import Any

try:
    import tkinter as tk
    from tkinter import messagebox
except Exception:
    tk = None
    messagebox = None


def safe_rmtree(path: Path) -> None:
    if not path.exists():
        return
    for root, dirs, files in os.walk(path):
        for name in dirs:
            try:
                os.chmod(os.path.join(root, name), stat.S_IWRITE)
            except Exception:
                pass
        for name in files:
            try:
                os.chmod(os.path.join(root, name), stat.S_IWRITE)
            except Exception:
                pass
    shutil.rmtree(path, ignore_errors=True)


def ensure_exists(path: Path, description: str) -> None:
    if not path.exists():
        raise FileNotFoundError(f"{description} not found: {path}")


def load_json(path: Path) -> dict[str, Any]:
    ensure_exists(path, "Config JSON")
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def save_json(path: Path, content: dict[str, Any]) -> None:
    path.write_text(json.dumps(content, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def apply_descriptor_overrides(descriptor: dict[str, Any], version: str | None, version_name: str | None, engine_version: str | None) -> dict[str, Any]:
    updated = dict(descriptor)
    if version is not None:
        try:
            updated["Version"] = int(version)
        except ValueError as exc:
            raise ValueError(f"--version must be integer, got {version!r}") from exc
    if version_name is not None:
        updated["VersionName"] = version_name
    if engine_version is not None:
        updated["EngineVersion"] = engine_version
    return updated


def write_uplugin(path: Path, descriptor: dict[str, Any]) -> None:
    path.write_text(json.dumps(descriptor, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def update_root_uplugin_version(plugin_root: Path, uplugin_name: str, version: int, version_name: str, engine_version: str) -> None:
    uplugin_path = plugin_root / uplugin_name
    if not uplugin_path.exists():
        return
    try:
        with uplugin_path.open("r", encoding="utf-8") as handle:
            content = json.load(handle)
        content["Version"] = version
        content["VersionName"] = version_name
        content["EngineVersion"] = engine_version
        uplugin_path.write_text(json.dumps(content, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    except Exception as exc:
        print(f"[WARNING] 无法同步保存版本号到根目录 uplugin: {exc}", file=sys.stderr)


def write_filter_plugin(path: Path, include_paths: list[str]) -> None:
    lines = ["[FilterPlugin]"] + include_paths
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_version_triplet(version_name: str) -> tuple[int, int, int] | None:
    match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", version_name.strip())
    if not match:
        return None
    return int(match.group(1)), int(match.group(2)), int(match.group(3))


def bump_descriptor_version(descriptor: dict[str, Any]) -> dict[str, Any]:
    updated = dict(descriptor)
    updated["Version"] = int(updated.get("Version", 0)) + 1
    version_name = str(updated.get("VersionName", "1.0.0"))
    triplet = parse_version_triplet(version_name)
    if triplet is None:
        updated["VersionName"] = f"{version_name}.1"
    else:
        major, minor, patch = triplet
        updated["VersionName"] = f"{major}.{minor}.{patch + 1}"
    return updated


def copy_required_dirs(plugin_root: Path, staging_dir: Path, include_dirs: list[str]) -> None:
    for folder in include_dirs:
        src = plugin_root / folder
        ensure_exists(src, f"Required directory '{folder}'")
        dst = staging_dir / folder

        def ignore_large_models(dir_path, names):
            ignored = []
            rel_dir = Path(dir_path).relative_to(plugin_root)
            if rel_dir == Path("Resources/Models") or "Resources/Models" in rel_dir.parts:
                for name in names:
                    full_path = Path(dir_path) / name
                    if full_path.is_file():
                        if not name.endswith(".json"):
                            ignored.append(name)
            return ignored

        shutil.copytree(src, dst, dirs_exist_ok=True, ignore=ignore_large_models)


def prune_by_extensions(root_dir: Path, extensions: list[str]) -> int:
    normalized = {ext.lower() if ext.startswith(".") else f".{ext.lower()}" for ext in extensions}
    removed = 0
    for path in root_dir.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() in normalized:
            path.unlink(missing_ok=True)
            removed += 1
    return removed


def count_cpp_classes(source_dir: Path) -> int:
    header_extensions = {".h", ".hpp", ".hh", ".hxx"}
    pattern = re.compile(r"^\s*class\s+(?:[A-Za-z_]\w*\s+)*([A-Za-z_]\w*)\b[^\n]*", re.MULTILINE)
    class_names: set[str] = set()

    for path in source_dir.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in header_extensions:
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for match in pattern.finditer(text):
            line = match.group(0)
            if line.rstrip().endswith(";"):
                continue
            class_names.add(match.group(1))

    return len(class_names)


def has_copyright_header(text: str, header: str) -> bool:
    normalized = text.lstrip("\ufeff")
    checked = 0
    for line in normalized.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        checked += 1
        if stripped == header:
            return True
        if checked >= 10:
            break
    return False


def normalize_source_copyright(source_dir: Path, header: str) -> int:
    updated_count = 0
    exts = {".h", ".hpp", ".cpp", ".cxx", ".inl", ".cs"}
    for path in source_dir.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in exts:
            continue
        original = path.read_text(encoding="utf-8")
        if has_copyright_header(original, header):
            continue
        path.write_text(f"{header}\n{original}", encoding="utf-8")
        updated_count += 1
    return updated_count



def apply_unity_build_fix(staging_dir: Path, export_cfg: dict[str, Any]) -> bool:
    if not export_cfg.get("disable_unity_build", True):
        return False

    build_cs_rel = export_cfg.get("module_build_cs", "Source/UmgMcp/UmgMcp.Build.cs")
    build_cs_path = staging_dir / build_cs_rel
    if not build_cs_path.exists():
        return False

    original = build_cs_path.read_text(encoding="utf-8")
    if "bUseUnity = false;" in original:
        return False

    insertion_anchor = "PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;"
    if insertion_anchor not in original:
        return False

    updated = original.replace(
        insertion_anchor,
        insertion_anchor + "\n\t\tbUseUnity = false;",
        1,
    )
    build_cs_path.write_text(updated, encoding="utf-8")
    return True



def run_export(
    plugin_root: Path,
    config_path: Path,
    output_dir: Path,
    *,
    version: str | None,
    version_name: str | None,
    engine_version: str | None,
    zip_name: str | None,
    clean: bool,
    auto_bump: bool,
) -> dict[str, Any]:
    config = load_json(config_path)
    export_cfg = config.get("export", {})
    uplugin_name = export_cfg.get("uplugin_name", "UmgMcp.uplugin")

    # 优先从根目录物理 .uplugin 加载完整的描述作为 Source of Truth，以防丢失运行时模块与依赖
    root_uplugin_path = plugin_root / uplugin_name
    if root_uplugin_path.exists():
        try:
            with root_uplugin_path.open("r", encoding="utf-8") as handle:
                descriptor = json.load(handle)
        except Exception as exc:
            print(f"[WARNING] 无法读取根目录 uplugin: {exc}, 将回退使用 JSON 配置中的描述", file=sys.stderr)
            descriptor = config.get("descriptor", {})
    else:
        descriptor = config.get("descriptor", {})
    export_cfg = config.get("export", {})
    technical_cfg = config.get("technical_information", {})

    if not descriptor:
        raise ValueError("Config must contain non-empty 'descriptor'.")

    include_dirs = export_cfg.get("include_dirs", ["Config", "Resources", "Source"])
    prune_extensions = export_cfg.get("prune_extensions", [".md"])
    filter_paths = export_cfg.get("filter_plugin_paths", ["/Config/...", "/Resources/...", "/Source/..."])
    uplugin_name = export_cfg.get("uplugin_name", "UmgMcp.uplugin")
    zip_name_prefix = export_cfg.get("zip_name_prefix", "UmgMcp_FabRelease")
    copyright_header = export_cfg.get("copyright_header", "// Copyright (c) 2025-2026 Winyunq. All rights reserved.")
    technical_filename = export_cfg.get("technical_filename", "FabTechnicalInformation.md")

    if clean and output_dir.exists():
        safe_rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    timestamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    staging_dir = output_dir / f"{zip_name_prefix}_{timestamp}"
    staging_dir.mkdir(parents=True, exist_ok=True)

    copy_required_dirs(plugin_root, staging_dir, include_dirs)
    pruned_files = prune_by_extensions(staging_dir, prune_extensions)

    final_descriptor = apply_descriptor_overrides(descriptor, version, version_name, engine_version)
    if auto_bump:
        final_descriptor = bump_descriptor_version(final_descriptor)

    write_uplugin(staging_dir / uplugin_name, final_descriptor)
    write_filter_plugin(staging_dir / "Config" / "FilterPlugin.ini", filter_paths)
    updated_files = normalize_source_copyright(staging_dir / "Source", copyright_header)
    unity_fix_applied = apply_unity_build_fix(staging_dir, export_cfg)

    cpp_class_count = count_cpp_classes(staging_dir / "Source")

    zip_base_name = zip_name if zip_name else f"{zip_name_prefix}_{timestamp}"
    zip_path = Path(shutil.make_archive(str(output_dir / zip_base_name), "zip", root_dir=staging_dir))

    if auto_bump:
        config["descriptor"] = final_descriptor
        save_json(config_path, config)
        update_root_uplugin_version(plugin_root, uplugin_name, final_descriptor.get("Version", 1), final_descriptor.get("VersionName", "1.0.0"), final_descriptor.get("EngineVersion", "5.7.0"))

    safe_rmtree(staging_dir)

    return {
        "config_path": config_path,
        "staging_dir": staging_dir,
        "uplugin_path": staging_dir / uplugin_name,
        "filter_path": staging_dir / "Config" / "FilterPlugin.ini",
        "copyright_updated": updated_files,
        "pruned_files": pruned_files,
        "zip_path": zip_path,
        "cpp_class_count": cpp_class_count,
        "unity_fix_applied": unity_fix_applied,
        "final_version": final_descriptor.get("Version"),
        "final_version_name": final_descriptor.get("VersionName"),
    }


def run_cloud_publish(
    plugin_root: Path,
    config_path: Path,
    output_dir: Path,
    cloud_path: Path,
    *,
    auto_bump: bool,
    progress_callback: Any = None,
) -> list[Path]:
    try:
        cloud_path.mkdir(parents=True, exist_ok=True)
    except Exception as exc:
        raise RuntimeError(f"Cannot access or create cloud drive path {cloud_path}.\nEnsure Google Drive is mounted and G:\\ is accessible.\nDetail: {exc}")

    config = load_json(config_path)
    export_cfg = config.get("export", {})
    uplugin_name = export_cfg.get("uplugin_name", "UmgMcp.uplugin")

    # 优先从根目录物理 .uplugin 加载完整的描述作为 Source of Truth，以防丢失运行时模块与依赖
    root_uplugin_path = plugin_root / uplugin_name
    if root_uplugin_path.exists():
        try:
            with root_uplugin_path.open("r", encoding="utf-8") as handle:
                descriptor = json.load(handle)
        except Exception as exc:
            descriptor = config.get("descriptor", {})
    else:
        descriptor = config.get("descriptor", {})
    export_cfg = config.get("export", {})
    technical_cfg = config.get("technical_information", {})

    if not descriptor:
        raise ValueError("Config must contain non-empty 'descriptor'.")

    include_dirs = export_cfg.get("include_dirs", ["Config", "Resources", "Source"])
    prune_extensions = export_cfg.get("prune_extensions", [".md"])
    filter_paths = export_cfg.get("filter_plugin_paths", ["/Config/...", "/Resources/...", "/Source/..."])
    uplugin_name = export_cfg.get("uplugin_name", "UmgMcp.uplugin")
    zip_name_prefix = export_cfg.get("zip_name_prefix", "UmgMcp_FabRelease")
    copyright_header = export_cfg.get("copyright_header", "// Copyright (c) 2025-2026 Winyunq. All rights reserved.")
    technical_filename = export_cfg.get("technical_filename", "FabTechnicalInformation.md")

    if auto_bump:
        descriptor = bump_descriptor_version(descriptor)
        config["descriptor"] = descriptor
        save_json(config_path, config)
        update_root_uplugin_version(
            plugin_root,
            uplugin_name,
            descriptor.get("Version", 1),
            descriptor.get("VersionName", "1.0.0"),
            descriptor.get("EngineVersion", "5.7.0")
        )

    version = int(descriptor.get("Version", 1))
    version_name = str(descriptor.get("VersionName", "1.0.0"))

    timestamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    staging_dir = output_dir / f"UmgMcp_FabRelease_Staging_{timestamp}"
    staging_dir.mkdir(parents=True, exist_ok=True)

    copied_zips = []
    try:
        copy_required_dirs(plugin_root, staging_dir, include_dirs)
        prune_by_extensions(staging_dir, prune_extensions)
        write_filter_plugin(staging_dir / "Config" / "FilterPlugin.ini", filter_paths)
        normalize_source_copyright(staging_dir / "Source", copyright_header)
        apply_unity_build_fix(staging_dir, export_cfg)
        cpp_class_count = count_cpp_classes(staging_dir / "Source")

        ue_versions = ["5.6", "5.7", "5.8"]

        for index, ver in enumerate(ue_versions, start=1):
            if progress_callback:
                progress_callback(index, len(ue_versions), ver)

            zip_name = f"UmgMcp_FabRelease_{ver}"
            engine_version = f"{ver}.0"

            ver_descriptor = dict(descriptor)
            ver_descriptor["Version"] = version
            ver_descriptor["VersionName"] = version_name
            ver_descriptor["EngineVersion"] = engine_version

            write_uplugin(staging_dir / uplugin_name, ver_descriptor)

            zip_path = Path(shutil.make_archive(str(output_dir / zip_name), "zip", root_dir=staging_dir))

            dst_dir = cloud_path / ver
            dst_dir.mkdir(parents=True, exist_ok=True)
            dst_zip_path = dst_dir / f"{zip_name}.zip"

            if dst_zip_path.exists():
                dst_zip_path.unlink(missing_ok=True)
            shutil.copy2(zip_path, dst_zip_path)
            copied_zips.append(dst_zip_path)

    finally:
        safe_rmtree(staging_dir)

    return copied_zips



def copy_technical_info_to_clipboard(root_widget: tk.Tk, technical_cfg: dict[str, Any], cpp_class_count: int) -> str:
    features = technical_cfg.get("features", [])
    lines = [
        "Features:",
        "",
    ]
    for index, feature in enumerate(features, start=1):
        lines.append(f"{index}. {feature}")
        lines.append("")
    
    lines.extend([
        "",
        "Technical Details:",
        "",
        f"- Code Modules: {technical_cfg.get('code_modules', 'UmgMcp (Editor), LiteRTLMUnreal (Runtime)')}",
        f"- Number of Blueprints: {technical_cfg.get('number_of_blueprints', 0)}",
        f"- Number of C++ Classes: {cpp_class_count}",
        f"- Supported Development Platforms: {technical_cfg.get('supported_dev_platforms', 'Win64')}",
        f"- Supported Target Build Platforms: {technical_cfg.get('supported_target_platforms', 'Win64')}",
        f"- Documentation: {technical_cfg.get('documentation', 'https://github.com/winyunq/UnrealMotionGraphicsMCP')}",
    ])
    text_to_copy = "\n".join(lines)
    
    root_widget.clipboard_clear()
    root_widget.clipboard_append(text_to_copy)
    root_widget.update()
    return text_to_copy


def run_gui(plugin_root: Path, config_path: Path, output_dir: Path) -> int:
    if tk is None:
        raise RuntimeError("Tkinter is not available in this Python environment.")

    config = load_json(config_path)
    export_cfg = config.setdefault("export", {})
    uplugin_name = export_cfg.get("uplugin_name", "UmgMcp.uplugin")

    # 优先从根目录物理 .uplugin 加载完整的描述作为 Source of Truth，以防丢失运行时模块与依赖
    root_uplugin_path = plugin_root / uplugin_name
    if root_uplugin_path.exists():
        try:
            with root_uplugin_path.open("r", encoding="utf-8") as handle:
                descriptor = json.load(handle)
        except Exception:
            descriptor = config.setdefault("descriptor", {})
    else:
        descriptor = config.setdefault("descriptor", {})
    export_cfg = config.setdefault("export", {})
    technical_cfg = config.setdefault("technical_information", {})

    root = tk.Tk()
    root.title("Fab Export Manager")
    root.geometry("680x230")

    version_var = tk.StringVar(value=str(descriptor.get("Version", 1)))
    cloud_path_var = tk.StringVar(value=str(export_cfg.get("cloud_drive_path", "G:\\My Drive\\Fab Publish Auto\\UnrealMotionGraphicsMCP")))
    auto_bump_var = tk.BooleanVar(value=bool(export_cfg.get("auto_increment_version", True)))
    
    # 用于 Specs 状态栏显示的变量
    version_name_var = tk.StringVar(value=str(descriptor.get("VersionName", "1.0.0")))

    def save_config_from_fields() -> None:
        try:
            version_val = int(version_var.get().strip())
        except ValueError:
            version_val = int(descriptor.get("Version", 1))

        version_name_val = version_name_var.get().strip()
        engine_val = descriptor.get("EngineVersion", "5.7.0")

        descriptor["Version"] = version_val
        descriptor["VersionName"] = version_name_val
        export_cfg["auto_increment_version"] = bool(auto_bump_var.get())
        export_cfg["cloud_drive_path"] = cloud_path_var.get().strip()
        save_json(config_path, config)

        uplugin_name = export_cfg.get("uplugin_name", "UmgMcp.uplugin")
        update_root_uplugin_version(plugin_root, uplugin_name, version_val, version_name_val, engine_val)

    def update_specs_ui() -> None:
        v_name = version_name_var.get()
        c_count = count_cpp_classes(plugin_root / "Source")
        bp_count = technical_cfg.get("number_of_blueprints", 0)
        specs_label.config(text=f"VersionName: {v_name}   |   C++ Classes: {c_count}   |   Blueprints: {bp_count}")

    def on_export() -> None:
        try:
            save_config_from_fields()
            result = run_export(
                plugin_root,
                config_path,
                output_dir,
                version=None,
                version_name=None,
                engine_version=None,
                zip_name=None,
                clean=False,
                auto_bump=bool(auto_bump_var.get()),
            )
            version_var.set(str(result.get("final_version", "")))
            version_name_var.set(str(result.get("final_version_name", "")))
            
            update_specs_ui()

            # 自动拷贝最新的技术规格信息到剪贴板，提供极致发布体验
            try:
                copy_technical_info_to_clipboard(root, technical_cfg, result['cpp_class_count'])
                clip_status = "\n\n(已自动将最新的技术规格描述复制到剪贴板，您可直接在 Fab 商品后台 Ctrl+V 粘贴！)"
            except Exception:
                clip_status = ""

            messagebox.showinfo(
                "Fab Export",
                "Export finished.\n"
                f"C++ Classes: {result['cpp_class_count']}\n"
                f"Unity Fix: {'Applied' if result['unity_fix_applied'] else 'Skipped'}\n"
                f"Zip: {result['zip_path']}"
                f"{clip_status}",
            )
        except Exception as exc:
            messagebox.showerror("Fab Export", f"Export failed:\n{exc}")

    def on_publish_cloud() -> None:
        try:
            save_config_from_fields()
            cloud_p = Path(cloud_path_var.get().strip())
            confirm = messagebox.askyesno(
                "Publish to Cloud",
                f"This will batch export 9 zip packages (UE 5.0 to 5.8) and copy them to:\n{cloud_p}\n\nAre you sure you want to proceed?"
            )
            if not confirm:
                return

            try:
                cloud_p.mkdir(parents=True, exist_ok=True)
            except Exception as exc:
                messagebox.showerror("Publish to Cloud", f"Cannot access or create cloud drive path:\n{exc}")
                return

            current_version = int(version_var.get().strip())
            current_version_name = version_name_var.get().strip()

            ue_versions = ["5.6", "5.7", "5.8"]

            export_btn.config(state="disabled")
            publish_btn.config(state="disabled")
            close_btn.config(state="disabled")

            copied_files = []

            include_dirs = export_cfg.get("include_dirs", ["Config", "Resources", "Source"])
            prune_extensions = export_cfg.get("prune_extensions", [".md"])
            filter_paths = export_cfg.get("filter_plugin_paths", ["/Config/...", "/Resources/...", "/Source/..."])
            uplugin_name = export_cfg.get("uplugin_name", "UmgMcp.uplugin")
            zip_name_prefix = export_cfg.get("zip_name_prefix", "UmgMcp_FabRelease")
            copyright_header = export_cfg.get("copyright_header", "// Copyright (c) 2025-2026 Winyunq. All rights reserved.")
            technical_filename = export_cfg.get("technical_filename", "FabTechnicalInformation.md")

            timestamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
            staging_dir = output_dir / f"UmgMcp_FabRelease_Staging_{timestamp}"

            try:
                staging_dir.mkdir(parents=True, exist_ok=True)
                copy_required_dirs(plugin_root, staging_dir, include_dirs)
                prune_by_extensions(staging_dir, prune_extensions)
                write_filter_plugin(staging_dir / "Config" / "FilterPlugin.ini", filter_paths)
                normalize_source_copyright(staging_dir / "Source", copyright_header)
                apply_unity_build_fix(staging_dir, export_cfg)
                cpp_class_count = count_cpp_classes(staging_dir / "Source")
            except Exception as exc:
                safe_rmtree(staging_dir)
                export_btn.config(state="normal")
                publish_btn.config(state="normal", text="Publish to Cloud")
                close_btn.config(state="normal")
                messagebox.showerror("Publish to Cloud", f"Failed to initialize staging directory:\n{exc}")
                return

            def cleanup_and_restore() -> None:
                safe_rmtree(staging_dir)
                export_btn.config(state="normal")
                publish_btn.config(state="normal", text="Publish to Cloud")
                close_btn.config(state="normal")

            def process_next_version(index: int) -> None:
                if index >= len(ue_versions):
                    cleanup_and_restore()
                    update_specs_ui()
                    
                    try:
                        copy_technical_info_to_clipboard(root, technical_cfg, cpp_class_count)
                        clip_status = "\n\n(已自动将最新的技术规格描述复制到剪贴板，您可直接在 Fab 商品后台 Ctrl+V 粘贴！)"
                    except Exception:
                        clip_status = ""

                    msg = f"Batch publish completed successfully!{clip_status}\n\nCopied packages:\n"
                    for f in copied_files:
                        msg += f"- {f.name}\n"
                    messagebox.showinfo("Publish to Cloud", msg)
                    return

                ver = ue_versions[index]
                publish_btn.config(text=f"UE {ver} ({index+1}/9)...")
                root.update_idletasks()

                zip_name = f"UmgMcp_FabRelease_{ver}"
                engine_version = f"{ver}.0"

                try:
                    ver_descriptor = dict(descriptor)
                    ver_descriptor["Version"] = current_version
                    ver_descriptor["VersionName"] = current_version_name
                    ver_descriptor["EngineVersion"] = engine_version

                    write_uplugin(staging_dir / uplugin_name, ver_descriptor)

                    zip_path = Path(shutil.make_archive(str(output_dir / zip_name), "zip", root_dir=staging_dir))

                    dst_dir = cloud_p / ver
                    dst_dir.mkdir(parents=True, exist_ok=True)
                    dst_zip_path = dst_dir / f"{zip_name}.zip"

                    if dst_zip_path.exists():
                        dst_zip_path.unlink(missing_ok=True)
                    shutil.copy2(zip_path, dst_zip_path)
                    copied_files.append(dst_zip_path)

                except Exception as exc:
                    cleanup_and_restore()
                    messagebox.showerror("Publish to Cloud", f"Failed on UE {ver}:\n{exc}")
                    return

                root.after(50, lambda: process_next_version(index + 1))

            process_next_version(0)

        except Exception as exc:
            messagebox.showerror("Publish to Cloud", f"Initialization failed:\n{exc}")

    def on_close() -> None:
        try:
            save_config_from_fields()
        except Exception as exc:
            messagebox.showerror("Fab Export", f"Auto-save failed on exit:\n{exc}")
            return
        root.destroy()

    labels = [
        ("Version", version_var),
        ("Cloud Path", cloud_path_var),
    ]

    for row, (title, var) in enumerate(labels):
        tk.Label(root, text=title, anchor="w").grid(row=row, column=0, padx=12, pady=6, sticky="w")
        tk.Entry(root, textvariable=var, width=40).grid(row=row, column=1, padx=12, pady=6, sticky="we")

    tk.Checkbutton(root, text="Auto increment version on export", variable=auto_bump_var).grid(
        row=2, column=0, columnspan=2, padx=12, pady=6, sticky="w"
    )

    specs_frame = tk.Frame(root)
    specs_frame.grid(row=3, column=0, columnspan=2, padx=12, pady=4, sticky="we")

    specs_label = tk.Label(specs_frame, text="", fg="gray", anchor="w")
    specs_label.pack(side="left", fill="x", expand=True)

    def on_copy_tech_info() -> None:
        try:
            c_count = count_cpp_classes(plugin_root / "Source")
            copy_technical_info_to_clipboard(root, technical_cfg, c_count)
            copy_btn.config(text="Copied!", fg="green")
            root.after(1500, lambda: copy_btn.config(text="Copy Tech Info", fg="black"))
        except Exception as exc:
            messagebox.showerror("Copy Error", f"Failed to copy: {exc}")

    copy_btn = tk.Button(specs_frame, text="Copy Tech Info", command=on_copy_tech_info, padx=6)
    copy_btn.pack(side="right")

    update_specs_ui()

    button_bar = tk.Frame(root)
    button_bar.grid(row=4, column=0, columnspan=2, padx=12, pady=12, sticky="we")
    
    for c in range(3):
        button_bar.columnconfigure(c, weight=1)

    export_btn = tk.Button(button_bar, text="Export", command=on_export)
    export_btn.grid(row=0, column=0, padx=4, sticky="we", ipady=2)
    
    publish_btn = tk.Button(button_bar, text="Publish to Cloud", command=on_publish_cloud)
    publish_btn.grid(row=0, column=1, padx=4, sticky="we", ipady=2)
    
    close_btn = tk.Button(button_bar, text="Close", command=on_close)
    close_btn.grid(row=0, column=2, padx=4, sticky="we", ipady=2)

    root.protocol("WM_DELETE_WINDOW", on_close)
    root.grid_columnconfigure(1, weight=1)
    root.mainloop()
    return 0


def main() -> int:
    plugin_root = Path(__file__).resolve().parent
    default_config = plugin_root / "fab_export.json"
    default_output = plugin_root / "Saved" / "FabRelease"

    parser = argparse.ArgumentParser(description="Export Fab-ready zip package (Config/Resources/Source/UmgMcp.uplugin only).")
    parser.add_argument("--config", default=str(default_config), help="Path to export config JSON.")
    parser.add_argument("--output-dir", default=str(default_output), help="Output directory for staging and zip.")
    parser.add_argument("--version", help="Override descriptor Version (integer).")
    parser.add_argument("--version-name", help="Override descriptor VersionName.")
    parser.add_argument("--engine-version", help="Override descriptor EngineVersion.")
    parser.add_argument("--zip-name", help="Override zip file name without .zip.")
    parser.add_argument("--clean", action="store_true", help="Remove output directory before export.")
    parser.add_argument("--gui", action="store_true", help="Open GUI editor/exporter.")
    parser.add_argument("--cli", action="store_true", help="Run in command-line mode (GUI is default).")
    parser.add_argument("--no-auto-bump", action="store_true", help="Do not auto increment version on export.")
    parser.add_argument("--publish-cloud", action="store_true", help="Run batch publish to cloud drive directly.")
    parser.add_argument("--cloud-path", help="Override cloud drive path.")
    args = parser.parse_args()

    config_path = Path(args.config).resolve()
    output_dir = Path(args.output_dir).resolve()

    if args.publish_cloud:
        config = load_json(config_path)
        export_cfg = config.setdefault("export", {})
        cloud_path_str = args.cloud_path if args.cloud_path else export_cfg.get("cloud_drive_path", "G:\\My Drive\\Fab Publish Auto\\UnrealMotionGraphicsMCP")
        cloud_path = Path(cloud_path_str).resolve()
        auto_bump = not args.no_auto_bump and bool(export_cfg.get("auto_increment_version", True))

        print(f"[*] Starting batch publish to cloud: {cloud_path}")

        def cli_progress(current: int, total: int, ver: str) -> None:
            print(f"  [{current}/{total}] Packaging and copying UE {ver}...")

        try:
            copied_files = run_cloud_publish(
                plugin_root,
                config_path,
                output_dir,
                cloud_path,
                auto_bump=auto_bump,
                progress_callback=cli_progress,
            )
            print("[OK] Batch cloud publish finished!")
            for f in copied_files:
                print(f"  -> {f}")
            return 0
        except Exception as exc:
            print(f"[ERROR] Batch cloud publish failed: {exc}", file=sys.stderr)
            return 1

    if args.gui or not args.cli:
        return run_gui(plugin_root, config_path, output_dir)

    config = load_json(config_path)
    export_cfg = config.get("export", {})
    auto_bump = not args.no_auto_bump and bool(export_cfg.get("auto_increment_version", True))

    result = run_export(
        plugin_root,
        config_path,
        output_dir,
        version=args.version,
        version_name=args.version_name,
        engine_version=args.engine_version,
        zip_name=args.zip_name,
        clean=args.clean,
        auto_bump=auto_bump,
    )

    print("[OK] Fab package exported")
    print(f"  Config:      {result['config_path']}")
    print(f"  Staging:     {result['staging_dir']}")
    print(f"  Uplugin:     {result['uplugin_path']}")
    print(f"  Filter:      {result['filter_path']}")
    print(f"  C++ Classes: {result['cpp_class_count']}")
    print(f"  Unity Fix:   {'Applied' if result['unity_fix_applied'] else 'Skipped'}")
    print(f"  Tech Info:   {result['technical_info_path']}")
    print(f"  Pruned:      {result['pruned_files']} files")
    print(f"  Copyright+:  {result['copyright_updated']} source files")
    print(f"  Version:     {result['final_version']} ({result['final_version_name']})")
    print(f"  Zip:         {result['zip_path']}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        raise SystemExit(1)
