"""Importa los FBX de D:/BlenderProjects/NDE_FBX a Content/_NDE_/Props.

Ejecutar dentro del editor de Unreal (Output Log > Python, o Tools > Execute Python Script).
Por cada mesh crea:
    /Game/_NDE_/Props/<Categoria>/<SM_Nombre>/SM_Nombre
    /Game/_NDE_/Props/<Categoria>/<SM_Nombre>/Materials/
    /Game/_NDE_/Props/<Categoria>/<SM_Nombre>/Textures/
"""
import os
import re
import unreal

FBX_ROOT = r"D:\BlenderProjects\NDE_FBX"
DEST_ROOT = "/Game/_NDE_/Props"
ONLY = None  # ej: ["SM_Door_M"] para probar con una sola pieza

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def build_options():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", True)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("automated_import_should_detect_type", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = ui.static_mesh_import_data
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", True)
    sm.set_editor_property("generate_lightmap_u_vs", True)
    return ui


def import_one(path, category, name):
    dest = f"{DEST_ROOT}/{category}/{name}"
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", build_options())
    tools.import_asset_tasks([task])

    for asset_path in eal.list_assets(dest, recursive=False, include_folder=False):
        asset = eal.load_asset(asset_path)
        base = asset_path.rsplit("/", 1)[-1].split(".")[0]
        if isinstance(asset, unreal.MaterialInterface):
            clean = re.sub(r"^(M_|MI_|tripo_mat_)", "", base)
            new = f"{dest}/Materials/M_{name[3:]}_{clean}" if clean != name[3:] else f"{dest}/Materials/M_{name[3:]}"
            if "NDE_Glass" in base and isinstance(asset, unreal.Material):
                asset.set_editor_property("blend_mode", unreal.BlendMode.BM_TRANSLUCENT)
            eal.rename_asset(asset_path, new)
        elif isinstance(asset, unreal.Texture):
            suffix = "D"
            low = base.lower()
            if "normal" in low:
                suffix = "N"
            elif "rough" in low:
                suffix = "R"
            elif "metal" in low:
                suffix = "M"
            eal.rename_asset(asset_path, f"{dest}/Textures/T_{name[3:]}_{suffix}")
    eal.save_directory(dest, only_if_is_dirty=False, recursive=True)


def main():
    done, failed = 0, []
    for cat_dir in sorted(os.listdir(FBX_ROOT)):
        full = os.path.join(FBX_ROOT, cat_dir)
        if not os.path.isdir(full):
            continue
        category = re.sub(r"^\d+_", "", cat_dir)
        for f in sorted(os.listdir(full)):
            if not f.lower().endswith(".fbx"):
                continue
            name = f[:-4]
            if ONLY and name not in ONLY:
                continue
            try:
                import_one(os.path.join(full, f), category, name)
                done += 1
            except Exception as e:  # noqa
                failed.append((name, str(e)))
                unreal.log_warning(f"Fallo {name}: {e}")
    unreal.log(f"Importados: {done}. Fallidos: {len(failed)}")
    for n, e in failed:
        unreal.log_warning(f"  {n}: {e}")


main()
