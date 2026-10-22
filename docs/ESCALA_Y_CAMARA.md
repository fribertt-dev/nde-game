# Escala del Desvelado y cámara

## Escala (decisión)
Opción del GDD: **un Desvelado pequeño en una casa a escala de gigante**. La casa no cambia; el Desvelado y su cámara se miden respecto a sus ojos.

| Medida | Valor | Dónde |
|---|---|---|
| Desvelado (cápsula) | 44 cm de ancho × 68 cm de alto | NDE Scale |
| Ojos | 56 cm sobre el suelo, 10 cm por delante del centro | NDE Scale |
| Agachado | 44 cm de alto | NDE Scale |
| Escalón máximo | 25 cm | NDE Scale |
| Salto | 450 cm/s → sube unos 100 cm | NDE Scale |
| Andar / correr / agachado | 320 / 470 / 185 cm/s (GDD 3.3) | NDE Scale |
| Puerta Door_M | 160 × 220 cm (≈ 3,2 veces el Desvelado) | HouseGen |
| Techo | 380 cm; 400 cm por piso | HouseGen |
| Rampas | 26,6° como mucho; carriles de 170-360 cm; murete de 100 cm sobre la rampa | HouseGen |

Todo lo del personaje está en **Project Settings > Game > NDE Scale** (`UNDScaleSettings`, se guarda en `DefaultGame.ini`): se ajusta sin recompilar. El Desvelado lo aplica al crearse.

**Alternativa descartada**: bajar la casa a la escala del Desvelado (puertas de unos 60 cm). Quita la sensación de "casa de gigante" del GDD y obliga a rehacer HouseGen (celdas, módulos, tests).

Para capturas desde los ojos: `nde.Debug.PlacePlayer X Y Z Yaw Pitch` (servidor) y `nde.Debug.Delay <segundos> <comando>` (p. ej. `HighResShot 1280x720`).

`nde.Scale.Debug [segundos]` dibuja delante del jugador una regla con suelo, escalón, ojos, cápsula, salto, puerta, techo y piso de arriba, además del ancho de una puerta y la huella de la cápsula.

## Cámara
- **Primera persona** por defecto en la expedición y en la casa: la cámara va en los ojos, dentro de la cápsula (no atraviesa paredes ni techo) y gira con la vista.
  - Cabeceo limitado a ±85°; FOV 90 y plano cercano de 5 cm (NDE Scale).
  - Balanceo de cabeza de 1 cm al andar. Se quita con `bHeadBob` en NDE Scale o con `nde.Camera.HeadBob 0` (accesibilidad).
- **El cuerpo no cambia**: los demás jugadores lo ven entero. Solo se oculta al dueño la nariz del placeholder, que quedaría delante de los ojos. La nariz se inclina con el cabeceo replicado, así que los demás ven hacia dónde mira cada uno.
- `nde.Camera.FirstPerson`: `1` primera persona, `0` tercera persona (cámara de 180 cm, proporcional al Desvelado), `-1` automático (por defecto: tercera persona solo en La Madriguera).
- En red: el dueño avisa al servidor del modo (`ServerSetFirstPerson`) para que el cuerpo gire igual en los dos lados y no haya correcciones de posición.
