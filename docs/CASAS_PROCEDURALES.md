# Casas procedurales (HouseGen)

Código en `NDE/Source/NDE/HouseGen/`. Mapa de prueba: `Content/_NDE_/Levels/L_CasaPrueba`.

## Cómo funciona
1. **Lote** (`ANDHouseLot`): parcela de la calle. Su transform es el centro, `LotInfo.SizeCells` el tamaño máximo en celdas de 400 cm y `FrontDirection` el lado que da a la calle. Ninguna sala sale del lote ni del jardín delantero.
2. **Servidor**: genera un `FNDHouseGenerationManifest` con `FNDHouseGenerator` (determinista: misma semilla + `housegen-0.1` + mismo lote = misma casa). Si un intento no valida, prueba con una semilla derivada; si ninguno vale, usa una casa de respaldo fija.
3. **Red**: el manifiesto se replica en el lote. Los clientes no generan: construyen desde el manifiesto (`FNDHouseLayout::FromManifest`).
4. **Construcción**: cada sala es un `ANDRoomModuleActor` greybox (cubo del motor). Los huecos sin usar se tapan con pared. Más adelante cada módulo cargará su Level Instance de arte (`ArtLevel`).
5. **GameMode** (`ANDHouseGameMode`): igual que la expedición, pero los jugadores aparecen en la Entrada cuando la casa está construida.

## Reglas de H01 (provisionales)
- Una planta, distrito D01. Tamaño (`HouseSize` en el lote; `MinRooms`/`MaxRooms` lo cambian):

  | Tamaño | Salas | Lote mínimo | Bucles | 2.ª salida |
  |---|---|---|---|---|
  | Small | 5-8 | 10×10 | 0-2 | 50 % |
  | Medium | 7-11 | 12×12 | 0-2 | 60 % |
  | Large | 10-17 | 14×14 | 1-2 | 90 % |

  Si el lote es más pequeño que el mínimo, crece solo (lo avisa el log).
- Entrada en la fachada; distribuidor; dormitorio principal a 2-5 puertas; sala objetivo a 2-7.
- 18 piezas greybox en `NDRoomCatalog.cpp`: 2 vestíbulos, 3 pasillos (recto, esquina, T), sala de estar (normal y en L), cocina, comedor, dormitorio principal, baño, almacén, despensa, estudio, lavandería, biblioteca, habitación de juguetes y sala objetivo. Las salas en L usan `BlockedCells`.
- Puertas Door_M de 160 cm; bulto de 120 cm (la cápsula mide 44 cm). Los baños (Door_S, 100 cm) no están en la ruta del bulto.
- Mínimo 6 huecos de botín, 4 spawns en la Entrada, anchor de criatura en el dormitorio, extracción en el jardín delantero.

## Pisos y sótano
- Pisos: -1 sótano, 0 planta baja, 1 piso superior; en el manifiesto van en `Grid.Z`. Cada piso mide 400 cm (paredes de 380 + forjado de 20).
- Opciones del lote: `UpperFloor` y `Basement` (Auto según el tamaño, Never, Always). Con Auto: Small 10 % / 15 %, Medium 50 % / 30 %, Large 75 % / 40 %.
- La Entrada y la extracción siempre en planta baja. El piso superior y el sótano solo ocupan celdas con planta baja encima o debajo, y no tienen salida a la calle.
- Escalera = dos salas 2x1 iguales (`D01_Stairs_Bottom` abajo, `D01_Stairs_Top` arriba): rampa de 26,6° (360 cm de ancho, pasa el bulto) y hueco en el forjado. Abajo hay además una puerta a ras de suelo bajo el final de la rampa; por ahí entra una sala de planta baja que sostiene la llegada.
- El generador hace primero la planta baja entera y luego las escaleras. El dormitorio puede ir arriba (si cabe a profundidad ≤ 5); si no, se queda abajo. Las casas grandes intentan una segunda escalera unida arriba (bucle entre pisos), aunque casi nunca cabe.
- Sótano: la primera sala prefiere salir del pie de la rampa (la criatura puede bajar). Si no cabe, sale por debajo de la rampa: pasan los Desvelados, pero no la criatura.
- Techo solo donde no hay otra sala encima. `MaxVisibleFloor` en el lote oculta los pisos de arriba para ver los de abajo (0 = planta baja, -1 = sótano).
- El NavMeshBoundsVolume del mapa cubre de -600 a +1000 cm; bajo el lote no hay suelo del vecindario, para que el sótano no choque.
- Test `NDE.HouseGen.Floors`: 10.000 semillas por tamaño con los dos pisos extra forzados.

## Depuración
- `nde.HouseGen.Regenerate <semilla>` en la consola del host.
- `?Seed=1234` en la URL del mapa, o `bUseFixedSeed` en el lote.
- `bDrawDebugGraph` en el lote, o `ANDHouseGenDebugActor` con su botón **Generate**.
- Log: categoría `LogNDHouseGen`.
- Tests: `NDE.HouseGen.Connectivity` (10.000 semillas por tamaño), `NDE.HouseGen.Determinism`, `NDE.HouseGen.CarryEnvelope`, `NDE.HouseGen.Floors`.
- Log de la partida: `PathExists Entrada -> Dormitorio / Piso superior / Sotano`; si una generación necesita la casa de respaldo, el aviso dice por qué fallaron los intentos.

La navegación necesita `RuntimeGeneration=Dynamic` (está en `DefaultEngine.ini`) y un `NavMeshBoundsVolume` que cubra el lote.
