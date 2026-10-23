"""Importa las imágenes de la interfaz (pantalla de transición y fondo del menú).

Uso: en el editor de UE 5.7, File > Execute Python Script... y elige este archivo.
Lee los PNG de NDE/Art/UI y crea o actualiza /Game/_NDE_/UI/Textures/T_UI_Transicion_0N.
Son los que usa Project Settings > NDE UI > Loading Screen. Es idempotente.
"""

import os
import unreal

DEST = "/Game/_NDE_/UI/Textures"
SOURCE = os.path.join(unreal.Paths.project_dir(), "Art", "UI")
NAMES = ["T_UI_Transicion_01", "T_UI_Transicion_02", "T_UI_Transicion_03"]

tasks = []
for name in NAMES:
    path = os.path.join(SOURCE, name + ".png")
    if not os.path.exists(path):
        unreal.log_warning("NDE: falta " + path)
        continue
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for name in NAMES:
    asset_path = DEST + "/" + name
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not texture:
        continue
    # Imagen de interfaz: sin mips, sin streaming (se pinta durante la carga) y en sRGB.
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)

unreal.log("NDE: imágenes de la interfaz importadas en " + DEST)
