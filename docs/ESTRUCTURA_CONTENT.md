# Estructura de Content

Todo el contenido propio del juego vive en `NDE/Content/_NDE_/`. El guion bajo la mantiene arriba en el Content Browser y la separa de packs del Marketplace/Fab, plugins y contenido de plantillas, que quedan fuera de `_NDE_`.

```
Content/
├── _NDE_/                 Contenido propio del juego (core)
│   ├── Levels/            Niveles (.umap). Prefijo L_
│   ├── Characters/        Personajes jugables y criaturas gigantes
│   ├── Blueprints/        Lógica en Blueprint (GameMode, PlayerController, objetos interactivos)
│   ├── Props/             Objetos de las casas y botín
│   ├── Materials/         Materiales e instancias
│   ├── Textures/          Texturas
│   ├── Audio/             Sonidos y música
│   ├── UI/                Widgets y HUD
│   └── FX/                Niagara y efectos
└── (packs externos)       Fuera de _NDE_, sin modificar
```

## Prefijos
| Tipo | Prefijo | Ejemplo |
|---|---|---|
| Nivel | `L_` | `L_Test` |
| Blueprint | `BP_` | `BP_LootBag` |
| Static Mesh | `SM_` | `SM_Chair` |
| Skeletal Mesh | `SK_` | `SK_Raccoon` |
| Material | `M_` | `M_Wood` |
| Instancia de material | `MI_` | `MI_Wood_Dark` |
| Textura | `T_` | `T_Wood_D` |
| Widget | `WBP_` | `WBP_HUD` |
| Sonido | `S_` | `S_Footstep` |
| Niagara | `NS_` | `NS_Dust` |

Las subcarpetas son una propuesta inicial y pueden crecer según el GDD; la regla fija es que lo propio va dentro de `_NDE_`.
