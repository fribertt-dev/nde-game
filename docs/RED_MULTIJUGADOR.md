# Red y multijugador

Estado: **fase 1** (esqueleto de red). Base: GDD secciones 21, 22 y 25.

## Qué hay

| Clase | Carpeta | Qué hace |
|---|---|---|
| `UNDGameInstance` | `Core/` | Vive toda la ejecución. Vuelve al menú ante fallos de red o de viaje y guarda el motivo para la UI. Comandos de consola de prueba. |
| `UNDSessionSubsystem` | `Networking/` | Crear, buscar, unirse, invitar y salir de salas. Usa la interfaz genérica de sesiones de Unreal: Steam ahora, otras plataformas después. Sin Steam usa salas LAN. |
| `UNDNetworkSettings` | `Networking/` | Project Settings > Game > **NDE Network**: mapas, jugadores máximos y mínimos, cuenta atrás y versión de compatibilidad. |
| `ANDLobbyGameMode` / `ANDLobbyGameState` | `Networking/` | La Madriguera: rechaza versiones distintas y salas llenas, controla "listo" y lanza el viaje a la expedición. |
| `ANDExpeditionGameMode` / `ANDExpeditionGameState` | `Networking/` | Espera a que carguen todos, abre la partida, liquida una sola vez y vuelve a La Madriguera. Replica `RunGuid`, fase y hora de servidor de la fase. |
| `ANDPlayerState` | `Players/` | "Listo" replicado. |
| `ANDPlayerController` | `Players/` | RPC al servidor ("listo", salir). |

Flujo: `L_MenuPrincipal` → `NDHost` → `L_Madriguera` (listen server) → todos listos → `L_Expedicion_Test` (seamless travel) → fin → `L_Madriguera`.

Las reglas de la expedición (fases del 7.1, reloj de noche, botín, cuota y liquidación) son del frente **Bucle de expedición**, en `Expedition/`, `Loot/`, `Contracts/` y `Tools/`. `ANDExpeditionGameMode` se conectará a su `UNDExpeditionComponent` al integrar.

## Steam

- AppID **480** (Spacewar, el de pruebas de Valve) en `DefaultEngine.ini`. Al tener AppID propio, cambiar `SteamDevAppId`.
- Transporte: **SteamSockets**. Si Steam no está disponible (PIE o cliente Steam cerrado), cae a `IpNetDriver` y el subsistema NULL (LAN).
- Como el 480 lo comparten muchos juegos, las salas llevan la clave `NDE_GAME` y la búsqueda filtra por ella.

## Preparación única en el editor

1. Clic derecho en `NDE.uproject` > *Generate Visual Studio project files* y compilar `NDEEditor` (Development Editor).
2. En `Content/_NDE_/Levels/`, crear (duplicando `L_Test` o vacíos con suelo y un `PlayerStart`):
   - `L_MenuPrincipal`: World Settings > GameMode Override = `GameModeBase`.
   - `L_Madriguera`: GameMode Override = `NDLobbyGameMode`.
   - `L_Expedicion_Test`: GameMode Override = `NDExpeditionGameMode`.
3. Project Settings > Maps & Modes: *Game Default Map* = `L_MenuPrincipal`. Comprobar que *Game Instance Class* = `NDGameInstance`.
4. Project Settings > Game > NDE Network: revisar que los tres mapas apunten a esos niveles.

Aún no hay personaje propio: se usa el pawn por defecto (cámara voladora).

## Comandos de consola (tecla `º` o `~`)

| Comando | Dónde | Qué hace |
|---|---|---|
| `NDHost` / `NDHost 1` | Menú | Crea sala pública / solo amigos y abre La Madriguera |
| `NDFind` | Menú | Busca salas y las lista en el log (`LogNDNet`) |
| `NDJoin <n>` | Menú | Entra a la sala `n` de la última búsqueda |
| `NDInvite` | Sala | Abre el overlay de invitaciones de Steam |
| `NDReady` | Madriguera | Alterna "listo". Con todos listos, a los 3 s sale la expedición |
| `NDDebugReturnToLobby` | Expedición (host) | Termina la expedición y vuelve a La Madriguera (no existe en Shipping) |
| `NDLeave` | Cualquiera | Sale al menú |

## Pruebas

### A. En el editor (sin Steam)
1. Consola del editor: `net.AllowPIESeamlessTravel 1`.
2. Play > Net Mode **Play As Listen Server**, Number of Players **2**, abrir `L_Madriguera`.
3. `NDReady` en las dos ventanas → ambas viajan a `L_Expedicion_Test`.
4. En la ventana del host, `NDDebugReturnToLobby` → ambas vuelven a La Madriguera.
5. Cerrar la ventana del cliente estando listo → la cuenta atrás se cancela o sigue solo con el host.

### B. Standalone en un PC (LAN)
Lanzar dos veces con Steam cerrado: `UnrealEditor.exe NDE.uproject -game -log -windowed -ResX=1280 -ResY=720`.
En una, `NDHost`; en la otra, `NDFind` y `NDJoin 0`.

### C. Steam en dos PCs (lo importante)
Cada PC con Steam abierto y **cuentas distintas**. Lanzar como en B (o empaquetado).
- PC A: `NDHost`. Debe salir el overlay de Steam (Shift+Tab).
- PC B: `NDFind` y `NDJoin 0`, o aceptar la invitación de `NDInvite`.
- Repetir el viaje a la expedición y la vuelta.

### Qué mirar en el log (`LogNDNet`)
Sala creada, búsqueda con resultados, "Viajando a steam.…", "Expedicion … esperando jugadores", "Expedicion en marcha con N jugadores", "Volviendo a La Madriguera". Ante errores, el motivo ("Volviendo al menu (…)").

## Siguientes fases
2. Lobbies completos: votación de contrato, loadout, UI de búsqueda e invitaciones.
3. Patrones de autoridad e integración con el bucle de expedición.
4. Desconexión, rejoin, espectador a mitad de partida y caída del host.
5. `UNDProfileSubsystem` con guardado local.
6. Voz de proximidad como ruido (GDD 31).
7. Pruebas de red del 26.5 con latencia y pérdida simuladas.
