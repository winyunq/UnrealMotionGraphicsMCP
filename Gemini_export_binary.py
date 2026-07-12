import os
import shutil
import json
import zipfile
from pathlib import Path

def export_binary_plugin():
    plugin_root = Path("D:/UE5Project/FabUMGMCP/plugins/FabUmgMcp")
    export_root = plugin_root / "Saved" / "BinaryRelease"
    if export_root.exists():
        shutil.rmtree(export_root)
    export_root.mkdir(parents=True, exist_ok=True)

    staging_dir = export_root / "FabUmgMcp"
    staging_dir.mkdir()

    print(f"Starting export to {staging_dir}")

    # 1. Copy Folders
    folders_to_copy = ["Binaries", "Config", "Resources"]
    for folder in folders_to_copy:
        src = plugin_root / folder
        dst = staging_dir / folder
        if src.exists():
            print(f"Copying {folder}...")
            shutil.copytree(src, dst)
        else:
            print(f"Warning: {folder} not found")

    # 2. Cleanup Binaries (remove PDBs to save space)
    print("Cleaning up Binaries (removing .pdb files)...")
    for pdb in staging_dir.rglob("*.pdb"):
        pdb.unlink()

    # 3. Cleanup Resources/Models
    models_dir = staging_dir / "Resources" / "Models"
    target_model = "gemma-4-E2B-it-Q4_K_M.gguf"
    
    if models_dir.exists():
        print(f"Filtering models, keeping only {target_model}...")
        for item in models_dir.iterdir():
            if item.name != target_model:
                if item.is_file():
                    item.unlink()
                elif item.is_dir():
                    shutil.rmtree(item)
    
    # Verify target model exists in staging
    if not (models_dir / target_model).exists():
        print(f"Error: Target model {target_model} not found in staging!")
        # Attempt to copy it manually if it was missing or deleted
        src_model = plugin_root / "Resources" / "Models" / target_model
        if src_model.exists():
            models_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src_model, models_dir / target_model)
            print(f"Manually copied {target_model}")
        else:
            print(f"Critical Error: Source model {target_model} not found!")

    # 4. Copy UmgMcp.uplugin and modify it
    uplugin_src = plugin_root / "UmgMcp.uplugin"
    uplugin_dst = staging_dir / "UmgMcp.uplugin"
    if uplugin_src.exists():
        print("Copying and modifying .uplugin file...")
        with open(uplugin_src, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        # Set Installed to true for binary distribution
        data["Installed"] = True
        
        with open(uplugin_dst, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent='\t')
    else:
        print("Error: UmgMcp.uplugin not found!")

    # 5. Create ZIP
    zip_name = export_root / "FabUmgMcp_Binary_Release.zip"
    print(f"Creating ZIP archive: {zip_name}")
    
    with zipfile.ZipFile(zip_name, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, dirs, files in os.walk(staging_dir):
            for file in files:
                file_path = Path(root) / file
                arcname = file_path.relative_to(staging_dir.parent)
                zipf.write(file_path, arcname)

    print(f"Export completed successfully!")
    print(f"ZIP location: {zip_name}")

if __name__ == "__main__":
    export_binary_plugin()
