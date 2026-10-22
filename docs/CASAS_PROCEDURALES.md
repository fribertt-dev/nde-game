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
- El bucle de expedición usa estas marcas (`ANDHouseGameMode`, ver `docs/BUCLE_EXPEDICION.md`): el botín sale en los huecos según el contrato y la semilla (`HouseGen/NDHouseLoot`, prueba `NDE.HouseGen.LootPlan`), lo que pide el contrato va a la sala objetivo, y en la extracción va la zona de depósito con la camioneta greybox de lado.

## Puntos de juego (escondites, peligros, patrulla y sonido)
Salen del layout y de la semilla en `HouseGen/NDHouseGameplayPoints.cpp`, así que servidor y clientes tienen los mismos sin replicar nada más. Cada uno tiene `StableId` (su índice), que sirve de StableSpawnId.
- **Escondites** (`HidingSpots`): las marcas azules de cada módulo. Tipo según la sala: debajo de un mueble (dormitorios y sala de estar; caben 2), dentro de un armario (cocina, baño, almacén, despensa, lavadero; cabe 1) o detrás de un mueble (resto; cabe 1, se le ve de lado). Cada uno tiene su punto de entrada y salida hacia el centro de la sala.
- **Peligros** (`Hazards`): suelo que cruje, objeto frágil y trampa de ratón. Más o menos en una de cada dos salas libres, nunca en la ruta del bulto (entrada → objetivo), ni en escaleras, ni a menos de 140 cm de una puerta.
- **Patrulla** (`PatrolNodes`, `PatrolRoute`): un nodo por sala (en las escaleras, en el umbral de su puerta), unidos por las puertas por las que cabe la criatura. El recorrido sale del dormitorio, pasa por todas las salas y vuelve. En partida, el servidor comprueba con el NavMesh que se llega a cada nodo (`PatrolNodesStuck` y el log).
- **Sonido** (`AudioLinks`): una arista por puerta con cuánto sonido pasa: hueco o puerta abierta 0,6, paso ancho 0,85, escalera 0,7 (y 0,2 para una puerta cerrada, cuando las haya). `ANDHouseLot::GetSoundTransmission(A, B)` da la fracción que llega de un punto a otro por el mejor camino, sin contar la distancia.
- El validador pide al menos 2 escondites con entrada accesible, que la patrulla cubra todas las salas sin nodos aislados y que ningún peligro esté en la ruta del bulto ni tape una puerta. Prueba: `NDE.HouseGen.GameplayPoints` (600 casas, 9,8 escondites y 2 peligros de media).
- `nde.HouseGen.DebugPoints [segundos] [espera]` los dibuja encima de todo: escondites en azul (con capacidad y entrada), peligros en naranja, patrulla en rojo y el paso del sonido en cian.
- Cambió la versión de generación a `housegen-0.2` (el validador nuevo puede descartar casas que antes valían).

## Variantes de piezas y muebles
- Cada familia de salas de H01 tiene 2 o 3 variantes (las B de `housegen-0.3` cambian tamaño, puertas y huecos): cocina 2x1 o 2x2, comedor 2x1 o 1x1, baño 1x1 o 2x1, almacén, objetivo (la B tiene cuatro huecos, más accesos), despacho, lavadero, biblioteca, cuarto de juegos, despensa y dormitorio infantil. Las escaleras tienen sus tres tipos (recta, U y L). La prueba `NDE.HouseGen.VariantsAndDecorations` comprueba que cada familia tiene 2-5 variantes y que se usan los 39 módulos.
- Muebles (`Decorations`): 2-3 por sala elegidos con la semilla (no en entrada, pasillos ni escaleras): mesa, sillón, armario, estantería, cama pequeña o caja, según la sala. Van pegados a esquinas o paredes, con colisión simple, y nunca cerca de una puerta (incluido el hueco desplazado de las escaleras en U), en la línea puerta-centro de la sala ni encima del botín, los escondites o el borde de la cama de la criatura. Mesas y camas son escondites de dos; armarios y sillones, de uno. Con 400 casas: el 99 % de las salas tiene muebles (2,2 de media) y ningún nodo de patrulla queda atascado.
- `nde.HouseGen.Furniture 0` construye la casa sin muebles (depuración; los puntos de juego no cambian).
- La patrulla de la criatura no pasa por puertas pequeñas (Door_S, baños): esas salas quedan fuera del recorrido, pero cuentan para el sonido.
- El botín no recorta el NavMesh (se mueve y se lleva).

## Contratos de D01 (C01 a C07)
Cada contrato (`UNDContractDefinition`) trae `HouseRequirements`: salas obligatorias, dónde va el objetivo (sala objetivo, sala obligatoria, cabecera de la cama o tarro de rescate), 2-5 ubicaciones candidatas, accesos mínimos a esa sala (1-3 rutas), ancho mínimo de la ruta y el margen de botín sobre la cuota. El generador añade las salas obligatorias antes del relleno, abre bucles hacia el objetivo si faltan accesos y valida todo; el reparto del botín (`HouseGen/NDHouseLoot`) pone primero el objetivo en una de sus candidatas (la elige la semilla), luego los señuelos y después el relleno hasta cubrir la cuota con margen.

| Id | Nombre | Qué pide | Casa |
| --- | --- | --- | --- |
| C01 | La Primera Siesta | Cuota de 700 | Sala segura (escondite sin peligros), 6 huecos de botín |
| C02 | Oro de Cocina | Cuchara (L04) y azucarero (L05) de plata, cuota 500 | Cocina obligatoria; 2 quesos señuelo (L20) |
| C03 | El Reloj Perdido | Reloj cucú (L32) | 3 candidatas; 2 relojes señuelo (L33, L34) |
| C04 | Cuidado con la Vajilla | Tetera (L09), plato (L10) y jarrón (L11) intactos | Ruta de 160 cm; secundario: todos fuera antes del amanecer |
| C05 | La Hora del Ronquido | Medalla de oro (L41) | En la cabecera de la cama de la criatura, en el borde accesible |
| C06 | Rescate del Calcetín | Trasto (L50) | Dentro de un tarro (`ANDRescueContainer`) en una sala con 2 accesos; se abre quedándose 2 s al lado |
| C07 | El Televisor Prestado | Televisor (L16) | Ruta de bulto de 120 cm |

- Datos: `Scripts/nde_setup_contratos_d01.py` crea o actualiza los contratos y el botín nuevo (placeholder con formas del motor). Se ejecuta sin el editor abierto: `UnrealEditor-Cmd.exe NDE.uproject -run=pythonscript -script="<ruta>"`.
- Los señuelos y el botín de menos de 50 puntos no se usan de relleno, y lo que pide el contrato no se repite.
- Si una casa pequeña no tiene huecos para cubrir la cuota, el resto va en el suelo de salas de planta baja.
- Pruebas (cargan los Data Assets reales): `NDE.HouseGen.Contracts.TestQuotaCoverage`, `TestRescueContainer` y `TestCarryEnvelope`. Con 180 casas por contrato: 0 de respaldo, 0 objetivos inaccesibles, 0 cuotas cortas, 0 sin ruta de bulto, 0 tarros inválidos.
- Prueba en partida: `L_CasaPrueba?listen?Contract=C06`.

## Pisos y sótano
- Pisos: -1 sótano, 0 planta baja, 1 piso superior; en el manifiesto van en `Grid.Z`. Cada piso mide 400 cm (paredes de 380 + forjado de 20).
- Opciones del lote: `UpperFloor` y `Basement` (Auto según el tamaño, Never, Always). Con Auto: Small 10 % / 15 %, Medium 50 % / 30 %, Large 75 % / 40 %.
- La Entrada y la extracción siempre en planta baja. El piso superior y el sótano solo ocupan celdas con planta baja encima o debajo, y no tienen salida a la calle.
- Cada escalera son dos salas iguales (arranque abajo, llegada arriba) que suben 400 cm con tramos de 26,6° como mucho. El generador prueba la que quepa:
  - **Recta** 2x1 (`D01_Stairs_*`): una rampa de 360 cm de ancho; se sale arriba por el otro extremo. Abajo hay un paso bajo el final de la rampa (`bLowClearance`): pasan los Desvelados, no la criatura.
  - **En U** 2x1 (`D01_StairsU_*`): dos carriles de 170 cm con murete y rellano al fondo; la llegada queda encima de la entrada, sobre la sala de la que sale. Las puertas de los carriles van desplazadas (`Offset`) y la sala vecina abre el hueco en el mismo sitio.
  - **En L** 2x2 sin una celda (`D01_StairsL_*`): tramo, rellano y tramo girado 90°.
- El generador hace primero la planta baja entera y luego las escaleras. El dormitorio puede ir arriba (justo al llegar o una sala más allá, a profundidad ≤ 5); hay un dormitorio alargado 2x1 (`D01_MasterBedroom_B`) que cabe mejor arriba.
- La criatura tiene que llegar a toda la casa (sótano incluido) sin pasos bajos; si no, el layout se descarta.
- Casas grandes con piso superior: segunda escalera unida arriba (bucle entre pisos). Se cuelga de una sala de arriba o sale de la planta baja; si la sala vecina no tiene hueco en esa pared, se abre una puerta **"Auto"** (`Auto_X_Y_D` en el manifiesto: celda y lado del módulo sin girar).
- Casa Small con piso superior y sótano a la vez: sube a 10 salas (lo avisa el log).
- Techo solo donde no hay otra sala encima. `MaxVisibleFloor` en el lote oculta los pisos de arriba para ver los de abajo (0 = planta baja, -1 = sótano).
- El NavMeshBoundsVolume del mapa cubre de -600 a +1000 cm; bajo el lote no hay suelo del vecindario, para que el sótano no choque.
- Test `NDE.HouseGen.Floors`: 10.000 semillas por tamaño con los dos pisos extra forzados. Mide casa de respaldo, % de dormitorio arriba, % de segunda escalera unida arriba y % de sótano alcanzable por la criatura.

## Depuración
- `nde.HouseGen.Regenerate <semilla>` en la consola del host.
- `?Seed=1234` en la URL del mapa, o `bUseFixedSeed` en el lote.
- `bDrawDebugGraph` en el lote, o `ANDHouseGenDebugActor` con su botón **Generate**.
- Log: categoría `LogNDHouseGen`.
- Tests: `NDE.HouseGen.Connectivity` (10.000 semillas por tamaño), `NDE.HouseGen.Determinism`, `NDE.HouseGen.CarryEnvelope`, `NDE.HouseGen.Floors`.
- Log de la partida: `PathExists Entrada -> Dormitorio / Piso superior / Sotano`; si una generación necesita la casa de respaldo, el aviso dice por qué fallaron los intentos.

La navegación necesita `RuntimeGeneration=Dynamic` (está en `DefaultEngine.ini`) y un `NavMeshBoundsVolume` que cubra el lote.
