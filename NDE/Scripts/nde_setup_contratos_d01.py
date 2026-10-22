"""Crea o actualiza los contratos C01-C07 del distrito D01 y el botín que necesitan.

Uso (sin el editor abierto):
  UnrealEditor-Cmd.exe NDE.uproject -run=pythonscript -script="<ruta>/nde_setup_contratos_d01.py"
o en el editor: File > Execute Python Script...
Idempotente: si un asset existe, lo actualiza. El botín nuevo es placeholder (formas del motor).
"""

import unreal

DATA_ROOT = "/Game/_NDE_/Data"
LOOT_DIR = DATA_ROOT + "/Loot"
CONTRACT_DIR = DATA_ROOT + "/Contracts"

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
# Botín nuevo para los contratos (placeholder, sin arte).

C = unreal.NDLootCategory
F = unreal.NDFragility

LOOT = [
    # id, nombre, categoría, valor, fragilidad, forma, escala
    ("L04", "Cuchara de plata", C.POCKET, 140, F.STURDY, "Cylinder", (0.06, 0.06, 0.25)),
    ("L05", "Azucarero de plata", C.LIGHT, 210, F.DELICATE, "Cylinder", (0.18, 0.18, 0.20)),
    ("L09", "Tetera de porcelana", C.LIGHT, 200, F.CRITICAL, "Sphere", (0.25, 0.25, 0.22)),
    ("L10", "Plato pintado", C.LIGHT, 160, F.DELICATE, "Cylinder", (0.30, 0.30, 0.03)),
    ("L20", "Queso apestoso (señuelo)", C.POCKET, 10, F.STURDY, "Cube", (0.12, 0.10, 0.08)),
    ("L33", "Reloj de pared parado (señuelo)", C.MEDIUM, 60, F.DELICATE, "Cube", (0.30, 0.12, 0.40)),
    ("L34", "Despertador roto (señuelo)", C.LIGHT, 30, F.STURDY, "Cube", (0.15, 0.10, 0.15)),
    ("L41", "Medalla de oro", C.POCKET, 450, F.STURDY, "Cylinder", (0.08, 0.08, 0.01)),
    ("L50", "Trasto", C.LIGHT, 350, F.DELICATE, "Sphere", (0.30, 0.30, 0.30)),
]

for loot_id, name, category, value, fragility, mesh, scale in LOOT:
    data_asset("DA_Loot_" + loot_id, LOOT_DIR, unreal.NDLootDefinition, {
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
# Contratos C01-C07 con lo que piden a la casa.

O = unreal.NDObjectiveType
S = unreal.NDSecondaryObjective
A = unreal.NDContractAnchor
R = unreal.NDRoomType


def house(anchor=A.OBJECTIVE_ROOM, rooms=(), candidates=2, routes=1, width=0.0, coverage=1.35, safe_room=False):
    req = unreal.NDContractHouseRequirements()
    req.set_editor_property("required_rooms", list(rooms))
    req.set_editor_property("objective_anchor", anchor)
    req.set_editor_property("candidate_locations", candidates)
    req.set_editor_property("min_routes", routes)
    req.set_editor_property("min_route_width", width)
    req.set_editor_property("quota_coverage", coverage)
    req.set_editor_property("needs_safe_room", safe_room)
    return req


# Valores del GDD (12, 16.2): tier I (C01-C06) cuota 700-1.200, pago 120-170, reputación 35-55,
# 8-10 min; tier II (C07) cuota 1.100-2.000, pago 180-270, reputación 55-80, 9-12 min. Suben con la dificultad.
# Secundarios que aún no se pueden medir (sin criatura ni alertas) van con None y su texto.
CONTRACTS = [
    ("C01", "La Primera Siesta", "Extrae botín hasta cubrir la cuota. Hay una sala segura para esconderse.", 1, O.QUOTA, 700,
     [], False, [], house(safe_room=True),
     S.NO_FRAGILE_BROKEN, "Ningún jarrón roto", 120, 35, 600.0),
    ("C02", "Oro de Cocina", "Saca de la cocina la cuchara y el azucarero de plata. Cuidado con la comida que hace de cebo.", 1, O.QUOTA, 800,
     ["L04", "L05"], False, ["L20", "L20"], house(anchor=A.REQUIRED_ROOM, rooms=[R.R04_KITCHEN], candidates=4),
     S.NO_FRAGILE_BROKEN, "Ningún objeto frágil roto", 130, 40, 540.0),
    ("C03", "El Reloj Perdido", "Recupera el reloj cucú. Hay relojes parecidos que no valen nada.", 1, O.UNIQUE_ITEM, 850,
     ["L32"], False, ["L33", "L34"], house(candidates=3),
     S.NONE, "Sin tocar los otros relojes", 140, 45, 540.0),
    ("C04", "Cuidado con la Vajilla", "Lleva la tetera, el plato y el jarrón a la camioneta sin un solo golpe.", 1, O.UNIQUE_ITEM, 950,
     ["L09", "L10", "L11"], True, [], house(candidates=3, width=160.0),
     S.ALL_EXTRACTED_BEFORE_DAWN, "Tiempo récord: todos fuera antes del amanecer", 150, 45, 570.0),
    ("C05", "La Hora del Ronquido", "La medalla de oro está en la cabecera de la cama. El dueño duerme al lado.", 1, O.UNIQUE_ITEM, 1050,
     ["L41"], False, [], house(anchor=A.BED_HEADBOARD, candidates=2),
     S.NONE, "Como mucho 2 alertas por encima de ligero", 160, 50, 600.0),
    ("C06", "Rescate del Calcetín", "Trasto se quedó encerrado en un tarro. Ábrelo y llévalo a la camioneta.", 1, O.UNIQUE_ITEM, 1150,
     ["L50"], False, [], house(anchor=A.RESCUE_CONTAINER, routes=2, candidates=2),
     S.NONE, "Sin despertar a nadie", 170, 55, 600.0),
    ("C07", "El Televisor Prestado", "Lleva el televisor viejo hasta la camioneta.", 2, O.HEAVY_ITEM, 1400,
     ["L16"], False, [], house(width=120.0),
     S.REQUIRED_LOOT_INTACT, "El televisor llega sin golpes fuertes", 220, 65, 660.0),
]

for (cid, name, desc, tier, objective, quota, required, intact, decoys, house_req,
     secondary, secondary_text, payout, rep, night) in CONTRACTS:
    data_asset("DA_Contract_" + cid, CONTRACT_DIR, unreal.NDContractDefinition, {
        "contract_id": cid,
        "display_name": unreal.Text(name),
        "description": unreal.Text(desc),
        "difficulty_tier": tier,
        "objective_type": objective,
        "required_quota_value": quota,
        "required_loot_ids": required,
        "required_loot_must_be_intact": intact,
        "decoy_loot_ids": decoys,
        "house_requirements": house_req,
        "secondary_objective": secondary,
        "secondary_objective_text": unreal.Text(secondary_text),
        "base_payout_per_player": payout,
        "reputation_reward": rep,
        "night_duration_seconds": night,
        "dawn_grace_seconds": 45.0,
    })

unreal.log("NDE: contratos C01-C07 y su botin listos en " + DATA_ROOT)
