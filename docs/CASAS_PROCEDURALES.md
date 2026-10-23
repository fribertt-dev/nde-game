# Casas procedurales (HouseGen)

Código en `NDE/Source/NDE/HouseGen/`. Mapa de prueba: `Content/_NDE_/Levels/L_CasaPrueba`.

## Cómo funciona
1. **Lote** (`ANDHouseLot`): parcela de la calle. Su transform es el centro, `LotInfo.SizeCells` el tamaño máximo en celdas de 400 cm y `FrontDirection` el lado que da a la calle. Ninguna sala sale del lote ni del jardín delantero.
2. **Servidor**: genera un `FNDHouseGenerationManifest` con `FNDHouseGenerator` (determinista: misma semilla + `housegen-0.1` + mismo lote = misma casa). Si un intento no valida, prueba con una semilla derivada; si ninguno vale, usa una casa de respaldo fija.
3. **Red**: el manifiesto se replica en el lote. Los clientes no generan: construyen desde el manifiesto (`FNDHouseLayout::FromManifest`).
4. **Construcción**: cada sala es un `ANDRoomModuleActor`: cajas greybox (cubo del motor) para la colisión y, encima, las mallas de arte (ver "Mallas de arte"). Los huecos sin usar se tapan con pared.
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
- Muebles (`Decorations`, `HouseGen/NDHouseFurniture.cpp`): muebles de arte reales según el tipo de sala, en orden de prioridad (baño: váter, lavabo con espejo, bañera si cabe; cocina: nevera, horno, fregadero, encimeras, mesa; dormitorios: armario, mesillas con lámpara, cómoda o cama; sala: sofá, chimenea, mueble de TV, sillones; comedor: mesa con sillas, aparador, vitrina; entrada: zapatero, perchero y felpudo...). Cada mueble usa su tamaño real: va con el respaldo contra una pared (esquinas incluidas), en el centro (mesas, salas de 2 celdas o más) o plano en el suelo (alfombras, sin colisión). Su huella no tapa puertas, la línea puerta-centro de la sala, el centro, el botín, los escondites, los peligros ni el borde de la cama de la criatura. Mesas y camas son escondites de dos; armarios y sillones, de uno. Con 400 casas: el 100 % de las salas tiene muebles (5,3 de media).
- La cama de la criatura (cama doble) va sobre su ancla en el dormitorio principal, sin colisión hasta que la criatura tenga su arte; en el dormitorio alargado 2x1 no cabe.
- `nde.HouseGen.Furniture 0` construye la casa sin muebles (depuración; los puntos de juego no cambian).
- La patrulla de la criatura no pasa por puertas pequeñas (Door_S, baños): esas salas quedan fuera del recorrido, pero cuentan para el sonido.
- El botín no recorta el NavMesh (se mueve y se lleva).

## Mallas de arte (housegen-0.5)
- Celdas de 8 m (`CellSize` 800): cada lado de celda son dos piezas de 400 de las paredes de arte y cada suelo o techo son 2x2 baldosas. La altura no cambia (pisos de 600 cm). Las medidas del catálogo de salas, pensadas para celdas de 400, se escalan con `CellScale`.
- Salas, muebles y exterior se ven con las mallas de `Content/_NDE_/Props`. Las cajas greybox siguen debajo, invisibles, y dan la colisión y la navegación (las mallas importadas no traen colisión simple). `nde.HouseGen.Art 0` vuelve al greybox. Las mallas se cargan por ruta, por eso `/Game/_NDE_/Props` está en `DirectoriesToAlwaysCook`.
- Paredes: la pieza con puerta va centrada en el hueco (también los huecos desplazados de las escaleras en U) y la colisión sigue su hueco real: Door_S y paso secreto → `SM_Wall_Door_L_400` (134 cm), Door_M y puerta de la calle → `SM_Wall_Door_M_400` (168 cm), Door_L → `SM_Wall_Door_S_400` (244 cm), Arch_L → `SM_Wall_Arch_L_400` (256 cm). Ojo: el nombre de esas mallas no coincide con el ancho de su hueco.
- Paredes exteriores (el lote las marca con `ENDWallStyle`): fachada lisa, fachada con ventana o ventana real (`SM_Wall_Window_400` con marco y cortinas; la colisión queda cerrada). En el sótano, ventanucos. Aleros (`SM_Roof_Edge_400`) sobre las paredes exteriores sin piso encima y frontones (`SM_Roof_Gable_400`) hacia la calle.
- Suelo según la sala (baldosa, hormigón, moqueta o madera), techo, zócalos en todas las paredes, lámparas de techo, vigas en salas grandes, ductos y rejillas en baños, cocinas, lavaderos, pasillos y sótanos, columnas de piedra en sótano y trastero, pilares donde se juntan cuatro celdas, trampilla del desván en el pasillo de arriba, marcos en las puertas medianas y puertas cerradas en los huecos del módulo que la casa no abrió.
- Escaleras: `SM_Stairs_Giant` ajustada a cada tramo (recta, U y L), rellanos con tarima y muretes con pared.
- Exterior (`ANDHouseLot::BuildExterior`): césped, camino de piedra de la puerta a la calle, cerca con portón, arbustos junto a la casa y, fuera del lote, acera, bordillo, calzada, farolas y césped. Puerta de la calle abierta y camioneta con su malla (5 m).
- Peligros con su malla: tabla que cruje (`SM_Floor_Creaky_400`), botellas frágiles, juguete que chilla; tarro de captura del C06 (`SM_Capture_Jar`); herramientas de la banda junto a la camioneta.
- Botín: la ficha tiene `ArtMesh` (se ve a su tamaño real); la física sigue en la forma greybox, que queda oculta. 11 fichas con malla (reloj de bolsillo, monedas, llaves, cuchara y cucharón, tetera, plato, jarrón, lámpara, televisor, despertador).

### Dónde se usa cada malla de Props (128 de 130)
| Carpeta | Mallas | Dónde |
| --- | --- | --- |
| Structure | Wall_400, Wall_Door_S/M/L_400, Wall_Arch_L_400, Wall_Window_400, Facade_Wall/Window_400, Facade_FrontDoor, Floor_Wood/Tile/Carpet/Concrete/Creaky_400, Ceiling_400, Stairs_Giant, Baseboard_400, Window_Frame, Curtain_A, BasementWindow, Beam_Ceiling, Column, Column_Stone_A/B/C, Vent_Duct_Straight/T/Corner, Vent_Grate, AtticHatch_Ladder, Door_M, DoorFrame_M, Door_M_Framed, Roof_Edge_400, Roof_Gable_400 | Salas (`ANDRoomModuleActor`) y tejado (lote) |
| Living, Kitchen, Dining, Bedroom, Bathroom, Entry_Common | todas | Muebles y adornos por tipo de sala (`NDHouseFurniture`) |
| Exterior, Street | todas salvo Porch_Steps | Exterior del lote |
| Vehicles | Van_Desvelados | Camioneta (`ANDExtractionVan`) |
| Hazards | Bottles_Row, Toy_Squeaky, Capture_Jar | Peligros y tarro del C06 |
| Loot | las 8 | Fichas de botín (`ArtMesh`) |
| Tools | las 9 | Junto a la camioneta |

Sin usar: `SM_Porch_Steps` (la casa está a ras de suelo; unos escalones de 90 cm taparían la entrada) y `SM_Stairs_Ramp_Helper` (la escalera ya usa `SM_Stairs_Giant` y su rampa es la colisión greybox).

Faltan mallas: lavadora y secadora, banco de trabajo, baúl de juguetes, escritorio, zócalo con agujero (atajo de ratón) y puertas que se abren con su lógica. La cama de la criatura no tiene colisión hasta que la criatura tenga su arte.

## Calidad (10.000 semillas por tamaño y contrato)
Prueba `NDE.Quality.HouseGen` (fuera de las rápidas; tarda menos de un minuto): genera 10.000 casas por tamaño (Small, Medium, Large) con cada contrato real C01-C07, un tercio con piso superior y sótano forzados, reparte el botín y comprueba objetivo accesible, cuota con margen, ruta del bulto, escondites y patrulla. Deja en `Saved/HouseGenQuality/` el informe (`report.md`), las semillas con problemas (`failed_seeds.txt`) y un SVG por layout fallido o de respaldo (piso por columna; objetivo en amarillo, dormitorio en rojo, escaleras en gris, entrada en verde, puertas como puntos). Para probar con menos: `nde.Quality.Seeds 1000`.

Resultado con `housegen-0.5` (210.000 casas, celdas de 8 m y muebles de arte):

| Métrica | Resultado | Objetivo |
| --- | --- | --- |
| Casas de respaldo | 20 (0,01 %), todas C02 Small (cocina obligatoria en casa pequeña) | menos del 1 % |
| Objetivos inaccesibles | 0 | 0 |
| Cuota insuficiente | 0 | 0 |
| Bulto sin ruta | 0 | 0 |
| Escondites o patrulla inválidos | 0 | 0 |
| Tiempo medio de generación | 4,1 ms por casa (0,10 con `housegen-0.3`; lo que más cuesta es comprobar el paso de cada mueble) | |

Con piso superior: 38 % de las Small, 67 % de las Medium y 83 % de las Large; con sótano: 38 %, 34 % y 54 %. Para que C06 (dos accesos al tarro) nunca deje sin casa, si no se puede abrir un bucle hacia el objetivo se cuelga de él una sala pequeña por otra puerta.

## Entrada tardía y reconexión
Un cliente que entra a mitad de noche o vuelve tras desconectarse recibe lo mismo que los demás:
- **Casa:** el manifiesto (lote siempre relevante) y la construye igual; mismo hash. Las salas se crean con el mismo nombre en todas las máquinas (`NDRoom_<lote>_r<revisión>_<sala>`) y son estáticas, así el servidor puede referirse a su suelo sin que el cliente se quede atascado.
- **Botín:** cada objeto es siempre relevante y nunca duerme en red, así que llega con su estado real (suelto, llevado y por quién, en la mochila, bloqueado en el tarro). La física solo corre en el servidor; los clientes siguen la posición replicada y, cuando el objeto se queda quieto, su sitio de reposo (`RestLocation`), que también recibe quien entra tarde. Lo depositado llega por el libro de depósitos de la expedición; los objetos depositados ya no existen. LootGuid estable: nunca hay duplicados ni pérdidas.
- **Tarro de rescate:** su estado (abierto o cerrado) está replicado.
- **Expedición:** quien vuelve a tiempo recupera su PlayerState y vuelve a estar activo; quien entra nuevo en los primeros 90 s cuenta como participante.
- Aún no hay puertas que se abran ni escondites ocupables, así que no hay nada que sincronizar de ellos.

Comprobación: `nde.Expedition.StateHash <cada s> [1 = detalle]` escribe en cada máquina una huella del estado (botín, depósitos, tarros) y, con detalle, cada objeto con su posición y (en el servidor) si cada cliente tiene su canal abierto. `nde.Expedition.Later <s> <comando>` lanza un comando aunque se viaje o se desconecte (p. ej. `disconnect` y `open 127.0.0.1:7788`). Prueba hecha con servidor y tres conexiones (A desde el principio, se desconecta y vuelve; B entra tarde) mientras el host coge, lanza, deposita y abre el tarro: en 6 de 6 partidas, con tres casas distintas, las tres máquinas acaban con la misma huella y las posiciones a menos de 1 mm.

## Contratos de D01 (C01 a C07)
Cada contrato (`UNDContractDefinition`) trae `HouseRequirements`: salas obligatorias, dónde va el objetivo (sala objetivo, sala obligatoria, cabecera de la cama o tarro de rescate), 2-5 ubicaciones candidatas, accesos mínimos a esa sala (1-3 rutas), ancho mínimo de la ruta y el margen de botín sobre la cuota. El generador añade las salas obligatorias antes del relleno, abre bucles hacia el objetivo si faltan accesos y valida todo; el reparto del botín (`HouseGen/NDHouseLoot`) pone primero el objetivo en una de sus candidatas (la elige la semilla), luego los señuelos y después el relleno hasta cubrir la cuota con margen.

| Id | Nombre | Qué pide | Casa |
| --- | --- | --- | --- |
| C01 | La Primera Siesta | Cuota de 700; secundario: ningún jarrón roto | Sala segura (escondite sin peligros), 6 huecos de botín |
| C02 | Oro de Cocina | Cuchara (L04) y azucarero (L05) de plata, cuota 800 | Cocina obligatoria; 2 quesos señuelo (L20) |
| C03 | El Reloj Perdido | Reloj cucú (L32) y cuota 850; secundario: sin tocar los otros relojes (pendiente) | 3 candidatas; 2 relojes señuelo (L33, L34) |
| C04 | Cuidado con la Vajilla | Tetera (L09), plato (L10) y jarrón (L11) intactos y cuota 950 | Ruta de 160 cm; secundario: tiempo récord (todos fuera antes del amanecer) |
| C05 | La Hora del Ronquido | Medalla de oro (L41) y cuota 1.050; secundario: como mucho 2 alertas (pendiente) | En la cabecera de la cama de la criatura, en el borde accesible |
| C06 | Rescate del Calcetín | Trasto (L50) y cuota 1.150; secundario: sin despertar a nadie (pendiente) | Dentro de un tarro (`ANDRescueContainer`) en una sala con 2 accesos; se abre quedándose 2 s al lado |
| C07 | El Televisor Prestado | Televisor (L16) y cuota 1.400; secundario: sin golpes fuertes | Ruta de bulto de 120 cm |

Valores del GDD (12 y 16.2): C01-C06 son tier I (pago 120-170 por jugador, reputación 35-55, noche de 9-10 min) y C07 tier II (pago 220, reputación 65, 11 min); suben con la dificultad. Los secundarios marcados "pendiente" necesitan la criatura y el sistema de alertas; de momento no se evalúan. El bono por botín extra (tope 35 % del pago), el secundario (15 %), el rescate (7,5 %) y la devolución parcial (0,2) están en `FNDEconomyTuning`, dentro de los rangos del GDD.

- Datos: `Scripts/nde_setup_contratos_d01.py` crea o actualiza los contratos y el botín nuevo (placeholder con formas del motor). Se ejecuta sin el editor abierto: `UnrealEditor-Cmd.exe NDE.uproject -run=pythonscript -script="<ruta>"`.
- Los señuelos y el botín de menos de 50 puntos no se usan de relleno, y lo que pide el contrato no se repite.
- Si una casa pequeña no tiene huecos para cubrir la cuota, el resto va en el suelo de salas de planta baja.
- Pruebas (cargan los Data Assets reales): `NDE.HouseGen.Contracts.TestQuotaCoverage`, `TestRescueContainer` y `TestCarryEnvelope`. Con 180 casas por contrato: 0 de respaldo, 0 objetivos inaccesibles, 0 cuotas cortas, 0 sin ruta de bulto, 0 tarros inválidos.
- Prueba en partida: `L_CasaPrueba?listen?Contract=C06`.

## Pisos y sótano
- Pisos: -1 sótano, 0 planta baja, 1 piso superior; en el manifiesto van en `Grid.Z`. Cada piso mide 600 cm (paredes de 580 + forjado de 20), la altura de las paredes de arte; las puertas miden 480 cm.
- Opciones del lote: `UpperFloor` y `Basement` (Auto según el tamaño, Never, Always). Con Auto: Small 10 % / 15 %, Medium 50 % / 30 %, Large 75 % / 40 %.
- La Entrada y la extracción siempre en planta baja. El piso superior y el sótano solo ocupan celdas con planta baja encima o debajo, y no tienen salida a la calle.
- Cada escalera son dos salas iguales (arranque abajo, llegada arriba) que suben 600 cm con tramos de 36,9° como mucho. El generador prueba la que quepa:
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
- `nde.HouseGen.ViewRoom <sala o tipo> [yaw]` (host): cámara en lo alto de una sala para ver sus muebles; p. ej. `nde.HouseGen.ViewRoom Kitchen 90`.
- Patrulla sobre el NavMesh con muebles de arte: 0 nodos atascados en 21 casas. Cada mueble deja al menos 45 cm a los lados de la ruta puerta-centro y no corta el paso de 40 cm de radio desde el centro a ninguna puerta.
- `?Seed=1234` en la URL del mapa, o `bUseFixedSeed` en el lote.
- `bDrawDebugGraph` en el lote, o `ANDHouseGenDebugActor` con su botón **Generate**.
- Log: categoría `LogNDHouseGen`.
- Tests: `NDE.HouseGen.Connectivity` (10.000 semillas por tamaño), `NDE.HouseGen.Determinism`, `NDE.HouseGen.CarryEnvelope`, `NDE.HouseGen.Floors`.
- Log de la partida: `PathExists Entrada -> Dormitorio / Piso superior / Sotano`; si una generación necesita la casa de respaldo, el aviso dice por qué fallaron los intentos.

La navegación necesita `RuntimeGeneration=Dynamic` (está en `DefaultEngine.ini`) y un `NavMeshBoundsVolume` que cubra el lote.
