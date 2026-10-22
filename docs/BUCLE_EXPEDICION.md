# Bucle de expedición: corte gris

Primer corte jugable del bucle central: entrar en la casa, coger botín, llevarlo a la camioneta, votar la retirada y escapar antes del amanecer. Cubre los capítulos 7, 8, 12, 16 y 18 del GDD con valores provisionales. Todo el C++ está en `NDE/Source/NDE/`, en las carpetas `Expedition`, `Loot`, `Contracts`, `Tools` y `Tests`; el personaje es `Players/NDCharacter`.

## Cómo probarlo

1. Compila el proyecto en Visual Studio (`NDEEditor`, Development Editor) y abre el editor.
2. Ejecuta `File > Execute Python Script...` y elige `NDE/Scripts/nde_setup_bucle.py`. El script crea:
   - `/Game/_NDE_/Data/Loot`: 10 objetos de botín (L01, L02, L03, L07, L08, L11, L12, L16, L19 y L32).
   - `/Game/_NDE_/Data/Tools`: 6 herramientas (T01, T02, T03, T07, T17 y T18).
   - `/Game/_NDE_/Data/Contracts`: 3 contratos (C01, C03 y C07).
   - `/Game/_NDE_/Levels/L_Expedicion_Prueba`: exterior con camioneta y una casa de gigante con dos salas.
3. Abre `L_Expedicion_Prueba`. En `Play > Net Mode` elige **Play As Listen Server** con 2 a 4 jugadores y pulsa Play.
4. Para cambiar de contrato, selecciona `Director_Expedicion` y en el componente `Expedition` cambia `Contract`. Si quieres partidas cortas para probar, baja `NightDurationSeconds` en el Data Asset del contrato.

### En la casa generada (L_CasaPrueba)

`ANDHouseGameMode` monta el bucle solo: crea el director de expedición, reparte el botín en los huecos amarillos de la casa y pone la camioneta en la marca verde del jardín delantero. Cruzar la puerta de la entrada arranca el reloj.

- Contrato: `?Contract=C07` en la URL (por defecto C01). Ejemplo: `L_CasaPrueba?listen?Seed=42?Contract=C07`.
- Los objetos que pide el contrato (L16 en C07, L32 en C03) salen en la sala objetivo, que ya tiene ruta validada para el bulto. El resto se reparte al azar hasta pasar la cuota con un 50 % de margen; los pesados extra solo van en la planta baja.
- Todo sale de la semilla: misma casa y mismo botín (con el mismo LootGuid) en servidor y clientes. Solo el servidor crea los objetos.

Comandos de depuración (consola, `~`):

| Comando | Qué hace |
| --- | --- |
| `nde.Expedition.Status [cada s]` | Escribe en el log fase, reloj, botín y resultado. |
| `nde.Expedition.DepositLoot [cuántos] [espera]` | Servidor: deposita botín suelto en la camioneta. |
| `nde.Expedition.ToVan [espera]` | Servidor: lleva a todos a la camioneta. |
| `nde.Expedition.SkipTime <s> [espera]` | Servidor: adelanta el reloj de la noche. |
| `nde.Expedition.ToLoot [n] [espera]` | Servidor: pone al jugador mirando el objeto de botín número n. |
| `nde.Expedition.Shot [espera]` | Captura 1280x720 en `Saved/Screenshots` (también en un cliente que entra después). |
| `nde.Expedition.Grab <L16 o n> [jugador] [espera]` | Servidor: el jugador coge el objeto como con E. Dos veces con jugadores distintos = bulto compartido. |
| `nde.Expedition.Drop [jugador] [lanzar 0/1] [espera]` | Servidor: suelta o lanza. |
| `nde.Expedition.Walk [jugador] [cm] [espera]` | Servidor: el jugador avanza chocando con las paredes. |
| `nde.Expedition.Meet [cm] [espera]` | Servidor: pone al jugador 0 delante del 1, mirándose. |

### Botín en red

Cada máquina coloca lo que se lleva a partir del estado replicado (`Carriers` y `LootState` de `ANDLootActor`): con un portador va acoplado delante de él; con dos o más, entre ellos. La física solo corre cuando el objeto está suelto, y mientras alguien lo lleva el servidor no manda su posición. Al soltarlo o lanzarlo cae desde donde se ve. Si el portador desaparece (desconexión, muerte), lo que llevaba cae donde estaba. `nde.Expedition.Status` muestra en cada máquina dónde está cada objeto llevado y sus portadores.

## Controles del personaje provisional

| Acción | Teclado y ratón | Mando |
| --- | --- | --- |
| Mover y mirar | WASD y ratón | Sticks |
| Saltar, correr, agacharse | Espacio, Shift, Ctrl | A, L3, B |
| Coger botín / votar o huir en la camioneta | E | X |
| Soltar | G | Y |
| Lanzar (mantén para cargar) | Clic izquierdo | RT |
| Usar herramienta activa | Clic derecho | LT |
| Cambiar herramienta | 1 / 2 | Cruceta izq. / der. |
| Consumible rápido | Q | RB |
| Ping | Clic central | Cruceta arriba |
| Votar no | N | Cruceta abajo |

El estado de la expedición se ve como texto de depuración arriba a la izquierda (fase, reloj, botín, carga, herramientas, votación y resultado final).

## Qué reglas funcionan

- **Inserción (7.1):** el reloj empieza cuando el primero cruza la gatera o a los 20 s.
- **Reloj nocturno (7.6 y 24.9):** autoritativo con el tiempo del servidor. Fases: noche profunda (0 a 50 %), madrugada (50 a 80 %), pre-amanecer (80 a 100 %) con avisos a 120, 60, 30 y 10 s, amanecer y fin del día a los +45 s. La luz direccional del nivel sube al amanecer.
- **Botín (8.1 a 8.4):** cada objeto tiene un LootGuid. Los de bolsillo van a la mochila (3 huecos); los ligeros y medianos se llevan en las manos; los pesados necesitan 2 jugadores o el carrito (T07), y sin ayuda se arrastran muy despacio. Si los dos portadores se separan, el objeto se suelta. Los golpes dañan (65 % del valor) o rompen (20 %) según la fragilidad, y hacen ruido con `MakeNoise`. Si un objeto cae fuera del mapa vuelve a su último punto seguro, dañado.
- **Camioneta (5.7 y 7.9):** deposita automáticamente lo que llevas al entrar y lo que se lanza dentro. Cada LootGuid se deposita una sola vez.
- **Objetivo (12):** la cuota se mide en puntos válidos; los objetos requeridos tienen que llegar sin romperse.
- **Votación de escape (7.3):** desde la camioneta, 12 s, mayoría de supervivientes. Las abstenciones no bloquean y existe una opción de unanimidad. Un superviviente solo sale con una cuenta atrás de 5 s.
- **Amanecer (7.2 y 7.7):** se cancelan las votaciones; el primero que llega a la camioneta evacúa en 5 s sin votar. Al fin del día escapa quien esté en la camioneta y los demás se quedan atrás.
- **Resultado y liquidación (7.2 y 16.3):** perfecto, éxito, extracción parcial o fracaso. Las monedas y la reputación se calculan por jugador, una sola vez, con la fórmula del 16.3. Los capturados y los que se quedan atrás pierden los bonos personales, y quien abandona no gana reputación.
- **Herramientas (18):** linterna (T03), carrito pasivo (T07), ping (T02), espuma silenciosa (T17, amortigua el ruido y evita daños en su zona) y ratón de cuerda (T18, señuelo que hace ruido y rebota).

## Pruebas automáticas

En `Tools > Session Frontend > Automation`, filtra por `NDE.Expedition`. Las pruebas cubren las fases del reloj, el valor del botín, el bono por botín extra, el resultado y la recompensa por jugador.

## Puntos de integración

- **Red y multijugador:** `ANDExpeditionGameMode` debe localizar `UNDExpeditionComponent` (con `ANDExpeditionDirector::GetExpedition`), inyectar el RunGuid con `SetRunGuid()`, asignar el contrato con `SetContract()` antes de `StartExpedition()` y escuchar `OnExpeditionSettled`, que entrega un `FNDRunSummary`, para guardar los perfiles y volver a la Madriguera.
- **Sigilo e IA:** los objetos y los señuelos emiten ruido con `MakeNoise` (con su instigador). `ANDSilentFoamPatch::GetNoiseMultiplierAt()` sirve para amortiguar pasos. El gigante captura con `SetParticipantCaptured(PlayerState, true)` y la jaula rescata con `false`. Al ser capturado, el jugador suelta todo el botín.
- **Personaje:** `ANDCharacter` (y `BP_Desvelado`) ya lleva `UNDCarryComponent`, `UNDLoadoutComponent` y `UNDExpeditionPlayerComponent`. El texto de depuración está en `Expedition/NDExpeditionDebugHud`.

## Pendiente en este frente

- Madriguera mínima: tablón con votación de contrato, tienda de Don Mugre y bloqueo de loadout. Depende del lobby y del perfil del frente de red.
- Jaula de captura y rescate (7.4), cuando exista el gigante de prueba del frente de sigilo.
- HUD real (frente de interfaz). El texto de depuración actual es provisional.
