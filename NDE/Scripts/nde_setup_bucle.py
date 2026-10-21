"""Crea los datos y el nivel gris del bucle de expedición.

Uso: en el editor de UE 5.7, File > Execute Python Script... y elige este archivo.
Necesita el proyecto compilado (clases ND*) y el plugin Python Editor Script activo.
Es idempotente: si un asset ya existe, lo actualiza; si el nivel ya existe, no lo toca.
"""

import unreal

DATA_ROOT = "/Game/_NDE_/Data"
LOOT_DIR = DATA_ROOT + "/Loot"
CONTRACT_DIR = DATA_ROOT + "/Contracts"
TOOL_DIR = DATA_ROOT + "/Tools"
LEVEL_PATH = "/Game/_NDE_/Levels/L_Expedicion_Prueba"
LOBBY_PATH = "/Game/_NDE_/Levels/L_Madriguera_Prueba"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def shape(name):
    return unreal.load_asset("/Engine/BasicShapes/" + name)


def data_asset(name, folder, cls, props):
    path = folder + "/" + name
    if eal.does_asset_exist(path):
        asset = eal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", cls)
        asset = asset_tools.create_asset(name, folder, cls, factory)
    for key, value in props.items():
        asset.set_editor_property(key, value)
    eal.save_loaded_asset(asset)
    return asset


# ---------------------------------------------------------------------------
# Botín (GDD 8.5): 10 objetos del catálogo para el primer corte.

C = unreal.NDLootCategory
F = unreal.NDFragility

LOOT = [
    # id, nombre, categoría, valor, fragilidad, forma, escala
    ("L01", "Reloj de bolsillo", C.POCKET, 85, F.STURDY, "Cylinder", (0.12, 0.12, 0.04)),
    ("L02", "Monedas antiguas", C.POCKET, 70, F.STURDY, "Cylinder", (0.10, 0.10, 0.05)),
    ("L03", "Llaves doradas", C.POCKET, 120, F.STURDY, "Cube", (0.12, 0.04, 0.02)),
    ("L07", "Cucharón de plata", C.LIGHT, 120, F.STURDY, "Cylinder", (0.08, 0.08, 0.40)),
    ("L08", "Candelabro", C.LIGHT, 180, F.DELICATE, "Cone", (0.20, 0.20, 0.40)),
    ("L11", "Jarrón pintado", C.LIGHT, 190, F.CRITICAL, "Cylinder", (0.25, 0.25, 0.40)),
    ("L12", "Lámpara antigua", C.MEDIUM, 250, F.DELICATE, "Cone", (0.40, 0.40, 0.60)),
    ("L32", "Reloj cucú", C.MEDIUM, 390, F.DELICATE, "Cube", (0.35, 0.20, 0.50)),
    ("L16", "Televisor viejo", C.HEAVY, 520, F.DELICATE, "Cube", (0.90, 0.60, 0.70)),
    ("L19", "Cofre tallado", C.HEAVY, 670, F.STURDY, "Cube", (0.90, 0.50, 0.50)),
]

loot_assets = {}
for loot_id, name, category, value, fragility, mesh, scale in LOOT:
    loot_assets[loot_id] = data_asset("DA_Loot_" + loot_id, LOOT_DIR, unreal.NDLootDefinition, {
        "loot_id": loot_id,
        "display_name": unreal.Text(name),
        "category": category,
        "base_value": value,
        "fragility": fragility,
        "mesh": shape(mesh),
        "mesh_scale": unreal.Vector(*scale),
        "impact_noise_loudness": 0.3 if category == C.POCKET else 0.6,
    })

# ---------------------------------------------------------------------------
# Herramientas (GDD 18): 6 del primer corte.

K = unreal.NDToolKind
E = unreal.NDToolEffect

TOOLS = [
    ("T01", "Manos de Desvelado", K.BASIC_FREE, E.NONE, 0, -1, 0.0, None),
    ("T02", "Ping silencioso", K.BASIC_FREE, E.PING, 0, -1, 4.0, None),
    ("T03", "Linterna de bolsillo", K.PERMANENT, E.FLASHLIGHT, 100, -1, 0.3, None),
    ("T07", "Carrito plegable", K.PERMANENT, E.CART, 350, -1, 0.0, None),
    ("T17", "Espuma silenciosa", K.CONSUMABLE, E.SPAWN_DEPLOYABLE, 35, 3, 1.0, unreal.NDSilentFoamPatch),
    ("T18", "Ratón de cuerda", K.CONSUMABLE, E.SPAWN_DEPLOYABLE, 50, 2, 2.0, unreal.NDWindUpMouse),
]

tool_assets = {}
for item_id, name, kind, effect, price, charges, cooldown, deploy_class in TOOLS:
    props = {
        "item_id": item_id,
        "display_name": unreal.Text(name),
        "kind": kind,
        "effect": effect,
        "price": price,
        "charges": charges,
        "cooldown_seconds": cooldown,
    }
    if deploy_class is not None:
        props["deployable_class"] = deploy_class.static_class()
    tool_assets[item_id] = data_asset("DA_Tool_" + item_id, TOOL_DIR, unreal.NDToolDefinition, props)

# ---------------------------------------------------------------------------
# Contratos (GDD 12 y 16.2): C01, C03 y C07.

O = unreal.NDObjectiveType
S = unreal.NDSecondaryObjective

CONTRACTS = [
    ("C01", "La Primera Siesta", "Extrae botín hasta cubrir la cuota.", 1, O.QUOTA, 700, [],
     S.NO_FRAGILE_BROKEN, "Ningún objeto frágil roto", 150, 45, 600.0),
    ("C03", "El Reloj Perdido", "Recupera el reloj cucú del salón.", 1, O.UNIQUE_ITEM, 0, ["L32"],
     S.ALL_EXTRACTED_BEFORE_DAWN, "Todos fuera antes del amanecer", 140, 40, 480.0),
    ("C07", "El Televisor Prestado", "Lleva el televisor viejo hasta la camioneta.", 2, O.HEAVY_ITEM, 0, ["L16"],
     S.REQUIRED_LOOT_INTACT, "El televisor llega sin golpes fuertes", 220, 65, 600.0),
]

contract_assets = {}
for cid, name, desc, tier, objective, quota, required, secondary, secondary_text, payout, rep, night in CONTRACTS:
    contract_assets[cid] = data_asset("DA_Contract_" + cid, CONTRACT_DIR, unreal.NDContractDefinition, {
        "contract_id": cid,
        "display_name": unreal.Text(name),
        "description": unreal.Text(desc),
        "difficulty_tier": tier,
        "objective_type": objective,
        "required_quota_value": quota,
        "required_loot_ids": required,
        "secondary_objective": secondary,
        "secondary_objective_text": unreal.Text(secondary_text),
        "base_payout_per_player": payout,
        "reputation_reward": rep,
        "night_duration_seconds": night,
        "dawn_grace_seconds": 45.0,
    })

unreal.log("NDE: datos del bucle creados en " + DATA_ROOT)

# ---------------------------------------------------------------------------
# Nivel gris: exterior con camioneta y una casa de gigante de dos salas.

if eal.does_asset_exist(LEVEL_PATH):
    unreal.log_warning("NDE: " + LEVEL_PATH + " ya existe; no se reconstruye. Bórralo para regenerarlo.")
else:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level_subsystem.new_level(LEVEL_PATH)

    def spawn(cls, loc, yaw=0.0, label=None):
        actor = actor_subsystem.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        if label:
            actor.set_actor_label(label)
        return actor

    def block(label, center, size, mesh="Cube"):
        actor = spawn(unreal.StaticMeshActor, center, label=label)
        actor.static_mesh_component.set_static_mesh(shape(mesh))
        actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
        return actor

    # Suelo exterior e interior. Escala de gigante: muros de 6 m (GDD 2.3).
    block("Suelo_Exterior", (0, 0, -10), (8000, 8000, 20))
    block("Suelo_Casa", (1500, 0, 1), (3000, 3000, 2))
    wall_h, wall_t = 600, 40
    block("Muro_Fondo", (3000, 0, wall_h / 2), (wall_t, 3000, wall_h))
    block("Muro_Norte", (1500, 1500, wall_h / 2), (3000, wall_t, wall_h))
    block("Muro_Sur", (1500, -1500, wall_h / 2), (3000, wall_t, wall_h))
    # Fachada con un hueco bajo (gatera de 80 cm) por donde entran los Desvelados.
    block("Fachada_Izq", (0, -800, wall_h / 2), (wall_t, 1400, wall_h))
    block("Fachada_Der", (0, 800, wall_h / 2), (wall_t, 1400, wall_h))
    block("Fachada_Dintel", (0, 0, (wall_h + 80) / 2), (wall_t, 200, wall_h - 80))
    # Tabique entre salón y cocina con un paso ancho para bultos.
    block("Tabique_A", (1500, -900, wall_h / 2), (wall_t, 1200, wall_h))
    block("Tabique_B", (1500, 900, wall_h / 2), (wall_t, 1200, wall_h))
    # Muebles de gigante como obstáculos y escondites.
    block("Mesa_Pata_1", (700, -900, 150), (40, 40, 300))
    block("Mesa_Pata_2", (1100, -900, 150), (40, 40, 300))
    block("Mesa_Tablero", (900, -900, 310), (600, 400, 20))
    block("Sofa", (900, 1000, 100), (900, 350, 200))
    block("Repisa_Baja", (2300, 1200, 20), (500, 200, 40))
    block("Encimera", (2600, -1200, 20), (600, 300, 40))
    block("Cama_Gigante", (2600, 300, 120), (600, 1000, 240))

    # Luces: noche fría fuera, interiores cálidos (GDD 7.6).
    sun = spawn(unreal.DirectionalLight, (0, 0, 2000), label="Luz_Noche")
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=25.0, yaw=-40.0), False)
    sun.light_component.set_intensity(0.15)
    sky = spawn(unreal.SkyLight, (0, 0, 1500), label="Cielo")
    sky.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.SkyAtmosphere, (0, 0, 0), label="Atmosfera")
    spawn(unreal.ExponentialHeightFog, (0, 0, 0), label="Niebla")
    for index, loc in enumerate([(800, 0, 500), (2300, -700, 500), (2300, 800, 500)]):
        lamp = spawn(unreal.PointLight, loc, label="Lampara_%d" % index)
        lamp.light_component.set_intensity(3000.0)
        lamp.light_component.set_light_color(unreal.LinearColor(1.0, 0.75, 0.45, 1.0))
        lamp.light_component.set_attenuation_radius(1400.0)

    # Camioneta, director y salidas de jugadores.
    van = spawn(unreal.NDExtractionVan, (-1000, 0, 0), label="Camioneta")
    van.body.set_static_mesh(shape("Cube"))
    van.body.set_relative_scale3d(unreal.Vector(4.5, 2.6, 0.2))

    director = spawn(unreal.NDExpeditionDirector, (120, 0, 0), label="Director_Expedicion")
    director.set_editor_property("sky_light_source", sun)
    director.expedition.set_editor_property("contract", contract_assets["C01"])

    for index in range(4):
        spawn(unreal.PlayerStart, (-1500, -300 + index * 200, 60), label="Inicio_%d" % index)

    # Botín repartido por la casa.
    placements = [
        ("L01", (600, 500, 20)), ("L02", (2400, 1200, 60)), ("L03", (1200, -300, 20)),
        ("L07", (2600, -1150, 70)), ("L08", (2200, 1200, 70)), ("L11", (2700, -1250, 80)),
        ("L12", (500, -600, 40)), ("L32", (1900, 1300, 40)), ("L16", (2000, -400, 50)),
        ("L19", (2500, 1000, 40)), ("L02", (800, 1300, 20)), ("L07", (1300, 600, 30)),
    ]
    for index, (loot_id, loc) in enumerate(placements):
        loot = spawn(unreal.NDLootActor, loc, label="Botin_%s_%d" % (loot_id, index))
        loot.set_editor_property("definition", loot_assets[loot_id])

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.NDExpeditionGameMode.static_class())

    level_subsystem.save_current_level()
    unreal.log("NDE: nivel creado en " + LEVEL_PATH)

# ---------------------------------------------------------------------------
# Madriguera mínima para probar el viaje completo con el frente de red.

if not eal.does_asset_exist(LOBBY_PATH):
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level_subsystem.new_level(LOBBY_PATH)
    floor = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -10))
    floor.set_actor_label("Suelo_Madriguera")
    floor.static_mesh_component.set_static_mesh(shape("Cube"))
    floor.set_actor_scale3d(unreal.Vector(20, 20, 0.2))
    light = actor_subsystem.spawn_actor_from_class(unreal.PointLight, unreal.Vector(0, 0, 400))
    light.light_component.set_intensity(8000.0)
    light.light_component.set_light_color(unreal.LinearColor(1.0, 0.8, 0.55, 1.0))
    light.light_component.set_attenuation_radius(2500.0)
    actor_subsystem.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 600))
    for index in range(4):
        actor_subsystem.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-300, -300 + index * 200, 60))
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.NDLobbyGameMode.static_class())
    level_subsystem.save_current_level()
    unreal.log("NDE: Madriguera de prueba creada en " + LOBBY_PATH)

# Ajustes de red: la Madriguera y la casa de prueba (Project Settings > NDE Red).
settings = unreal.get_default_object(unreal.NDNetworkSettings)
settings.set_editor_property("lobby_map", unreal.load_asset(LOBBY_PATH))
settings.set_editor_property("expedition_test_map", unreal.load_asset(LEVEL_PATH))

ini_path = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_config_dir() + "DefaultGame.ini")
with open(ini_path, "r", encoding="utf-8") as ini:
    ini_text = ini.read()
if "[/Script/NDE.NDNetworkSettings]" not in ini_text:
    with open(ini_path, "a", encoding="utf-8") as ini:
        ini.write("\n[/Script/NDE.NDNetworkSettings]\n")
        ini.write("LobbyMap=" + LOBBY_PATH + "." + LOBBY_PATH.split("/")[-1] + "\n")
        ini.write("ExpeditionTestMap=" + LEVEL_PATH + "." + LEVEL_PATH.split("/")[-1] + "\n")
    unreal.log("NDE: mapas de red guardados en DefaultGame.ini")
else:
    unreal.log_warning("NDE: DefaultGame.ini ya tiene ajustes de red; revisa LobbyMap y ExpeditionTestMap en Project Settings > NDE Red.")
