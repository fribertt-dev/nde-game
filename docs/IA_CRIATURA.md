# Criatura gigante (IA)

Código en `NDE/Source/NDE/Monsters/`. Capítulos del GDD: 9 (sigilo), 10 (monstruos), 7.4 (captura), 7.7 (amanecer) y 24.3/24.7.

## Cómo probarlo
1. Abre `L_CasaPrueba` y juega como Listen Server (1-4 jugadores). Al construirse la casa, el servidor pone la criatura (M01, El Dormilón) en su cama del dormitorio principal y una jaula en una sala vecina.
2. Sin casa generada (otro mapa) arrastra `NDMonsterCharacter` al nivel: duerme donde la pongas. Hace falta NavMesh.
3. Ajustes sin recompilar: Project Settings > Game > **NDE Monsters** (activar, clase, Data Asset, jaula, rescate). Los números del monstruo están en un Data Asset `NDMonsterDefinition` (vacío = valores de M01), con tooltips en español.

## Qué hace
- **Sueño y ronquidos (9.3, 9.4):** la sospecha (0-100) decide sueño profundo, ligero, se revuelve y medio despierto, con histéresis. Ronca en ciclos de 5-9 s replicados por la hora del servidor: inspira (aviso, el pecho se hincha) y ronca 1,8-3 s; mientras ronca tus ruidos se oyen al 30 % (los muy fuertes casi no se tapan). El ronquido suena (sintetizado, sin assets) y el cartel `Zzz` late con él.
- **Oído (9.2):** escucha con AIPerception (Hearing) lo que mandan los demás sistemas con `ReportNoiseEvent` o `MakeNoise` (pasos, voz, botín, herramientas). Loudness 0-1 = escala 0-100 del GDD. Cuenta la distancia, las puertas de la casa (`GetSoundTransmission`) y el ronquido. Golpes del mismo jugador en 0,2 s cuentan como uno.
- **Tacto:** dormida, nota a quien esté pegado a ella.
- **Vista (9.5):** solo despierta (con un ojo abierto, poco y hacia el último ruido). Traza de visibilidad: muebles y paredes tapan. Agachado y escondido se ve menos (usa `UNDHidingComponent` del personaje si existe); correr y la linterna delatan. El aviso de "te veo" se llena en menos de un segundo de cerca; al llenarse, persigue.
- **Árbol de decisión** (`ANDMonsterAIController`): amanecer → manotazo → te ve → en la cama (sospecha) → investigar → buscar → volver a la cama. Nunca sabe dónde estás sin verte ni oírte.
- **Rutina:** cada 70-130 s de sueño tranquilo puede levantarse a dar una vuelta de 2-4 salas por la patrulla de la casa y vuelve a la cama.
- **Amanecer (7.7):** se despierta del todo y ronda la casa hasta el final. Captura solo si te ve y te alcanza.
- **Captura (7.4, 24.7):** aviso de 0,7 s (levanta la mano). Primer manotazo: aturdido 2,5 s y empujado. Segundo (en menos de 60 s): a la jaula, capturado en la expedición y suelta el botín. Un compañero libre junto a la jaula 8 s la abre (con ruido). Si atrapa a todos, la expedición termina (derrota).

## Comandos (consola, `~`)
| Comando | Qué hace |
| --- | --- |
| `nde.Monster.Debug 1` | Dibuja estado, sospecha, ruidos oídos, vista y destino. |
| `nde.Monster.Status` | Estado de cada criatura en el log. |
| `nde.Monster.Wake` / `nde.Monster.Sleep` | Despierta / manda a dormir. |
| `nde.Monster.Suspicion <0-100>` | Fija la sospecha. |
| `nde.Monster.Noise <0-100> [jugador]` | Ruido en la posición del jugador. |
| `nde.Monster.NightWalk` | Ronda nocturna ya. |
| `nde.Monster.ToBed [jugador]` | Pone al jugador junto a la cama. |
| `nde.Monster.Respawn` / `nde.Monster.Release` | Vuelve a crear la criatura / abre la jaula. |
| `nde.Monster.AI 0` | Congela la criatura. |

Todos aceptan una espera final en segundos. Pruebas: `NDE.Monster` en Session Frontend > Automation.

## Pendiente
- Cuerpo provisional con formas del motor: falta el skeletal mesh, el Animation Blueprint y los sonidos reales.
- El gigante no lleva en brazos al capturado: lo mete directamente en la jaula.
- Solo M01. M02-M08, máquina de sueños, mascotas y director de eventos van después.
- Behavior Tree o StateTree: el árbol está en C++; si hace falta editarlo en el editor se puede pasar a StateTree.
