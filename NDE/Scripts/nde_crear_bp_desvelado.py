# Crea /Game/_NDE_/Characters/BP_Desvelado (hijo de ANDCharacter) si no existe.
# Ejecutar desde el editor: Tools > Execute Python Script.
import unreal

FOLDER = "/Game/_NDE_/Characters"
NAME = "BP_Desvelado"

if unreal.EditorAssetLibrary.does_asset_exist(f"{FOLDER}/{NAME}"):
    unreal.log(f"{NAME} ya existe; no se toca.")
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.NDCharacter)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.Blueprint, factory)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log(f"Creado {FOLDER}/{NAME}")
