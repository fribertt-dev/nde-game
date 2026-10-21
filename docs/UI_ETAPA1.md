# UI, etapa 1: base C++ y datos simulados

Esta etapa deja la arquitectura de UI del GDD (capítulo 32) en C++. Las pantallas se diseñan después como Widget Blueprints (`WBP_`) que heredan de estas clases, y funcionan desde el primer día con datos simulados.

## Qué hay en `Source/NDE`

| Archivo | Para qué sirve |
|---|---|
| `UI/NDUIManagerSubsystem` | Punto único de entrada a la UI de cada jugador local: crea la raíz, abre pantallas por etiqueta, muestra toasts y entrega el proveedor de datos |
| `UI/Widgets/NDUIRootLayout` | Raíz con cuatro capas (Game, GameMenu, Menu, Modal), cada una una pila de Common UI |
| `UI/Widgets/NDUIBaseScreen` | Base de toda pantalla: modo de entrada, Atrás, estado de carga y refresco por evento |
| `UI/Widgets/NDUIBaseModal` | Confirmaciones y avisos. Atrás equivale a cancelar |
| `UI/Widgets/NDUIBaseList` | Pantallas con colección filtrable y estado vacío (tablón, tienda, inventario) |
| `UI/Widgets/NDUIBaseCard` | Tarjeta de contrato, herramienta o jugador |
| `UI/Widgets/NDUIButtonBase` | Botón con texto y estado de espera |
| `UI/Widgets/NDUIToastQueue` | Notificaciones en cola por prioridad |
| `UI/Data/NDUIViewModels.h` | Modelos de solo lectura: jugador, contrato, tienda, slot, reloj nocturno, voz, resultados |
| `UI/Data/NDUIDataProvider` | Interfaz de datos y comandos que usan las pantallas |
| `UI/Data/NDUIMockDataProvider` | Datos simulados sacados del GDD: 4 jugadores, contratos C01/C03/C05/C07, herramientas T03–T10, noche simulada de 10 min y 45 s de huida |
| `UI/Data/NDUIScreenRegistry` | `DA_UIScreenRegistry`: etiqueta de pantalla → WBP y capa, con validación en el editor |
| `UI/Data/NDUIThemeData` | `DA_UITheme_Master`: paletas de noche, día y alto contraste, tipografía, estilos, iconos y sonidos |
| `UI/NDUISettings` | Project Settings > Game > NDE UI |
| `UI/NDUITags` | Etiquetas `UI.Screen.*` de todas las pantallas del flujo |
| `UI/NDUIFunctionLibrary` | Reloj nocturno (tiempo restante, progreso, huida), formato `M:SS`, nombre de especie |
| `Core/NDPlayerController`, `Core/NDFrontEndGameMode` | El controlador local inicializa la UI y abre la pantalla inicial |
| `UI/Tests` | Pruebas automáticas del mock y del reloj (`NDE.UI.*` en Session Frontend) |

Reglas que ya cumple el código:
- Ni el GameMode ni el Level Blueprint crean widgets. Lo hace el PlayerController local a través del subsistema.
- Las pantallas refrescan por evento (`OnDataChanged`), sin Tick ni bindings por frame.
- La UI solo pide (`RequestPurchase`, `RequestVoteContract`...). El resultado llega por `OnCommandResult`, y en el mock cada respuesta va marcada como simulada y no se guarda nada.
- Cada especie es única por partida y no puedes marcarte listo sin especie.
- El reloj nunca muestra tiempo negativo y el amanecer lo decide el servidor (en el mock, un temporizador).

## Pasos en el editor

1. Compila el proyecto desde Visual Studio o Rider (se añadieron Common UI, UMG y GameplayTags).
2. **Entrada de Common UI.** Crea `Content/_NDE_/UI/Core/Data/DT_UIInputActions` (Data Table de `CommonInputActionDataBase`) con las filas `Confirm` y `Back`, y un Blueprint `BP_NDUIInputData` hijo de `CommonUIInputData` que las use. Asígnalo en Project Settings > Game > Common Input Settings > Input Data.
3. **Raíz.** Crea `Content/_NDE_/UI/Core/Blueprints/WBP_RootLayout` hijo de `NDUIRootLayout`. Dentro de un Overlay pon, en este orden, cuatro `CommonActivatableWidgetStack` llamados `Layer_Game`, `Layer_GameMenu`, `Layer_Menu` y `Layer_Modal`.
4. **Bases visuales.** En `Content/_NDE_/UI/Core/Blueprints/` crea `WBP_BaseButton` (hijo de `NDUIButtonBase`, con un `CommonTextBlock` llamado `Text_Label`) y `WBP_Confirm` (hijo de `NDUIBaseModal`, con `Text_Title`, `Text_Body`, `Button_Confirm` y `Button_Cancel`).
5. **Tema.** Crea `Content/_NDE_/UI/Core/Data/DA_UITheme_Master` (Data Asset de `NDUIThemeData`).
6. **Registro.** Crea `Content/_NDE_/UI/Core/Data/DA_UIScreenRegistry` (Data Asset de `NDUIScreenRegistry`). Añade al menos `UI.Screen.MainMenu` → `WBP_MenuMain` (capa Menu) y `UI.Screen.Confirm` → `WBP_Confirm` (capa Modal).
7. **Ajustes.** En Project Settings > Game > NDE UI asigna `RootLayoutClass`, `ScreenRegistry` y `Theme`. `DataProviderClass` ya apunta al mock.
8. **Mapa de menú.** Crea `Content/_NDE_/Levels/L_MainMenu` y en World Settings pon GameMode Override = `NDFrontEndGameMode`. Al darle a Play se abre `UI.Screen.MainMenu`.

## Cómo se arma una pantalla

1. Crea el WBP heredando de `NDUIBaseScreen`, `NDUIBaseList` o `NDUIBaseModal`, nunca de otra pantalla concreta.
2. En Class Defaults elige `InputMode` (Menu para menús, Game para el HUD) y, si quieres, `RefreshTopics` (por ejemplo `Shop` y `Profile` en la tienda). En el HUD desmarca `Is Back Handler` para que Atrás no lo cierre.
3. Implementa `BP_RefreshFromViewModel`: pide los datos con `Get Data Provider` (por ejemplo `Get Shop Items`) y pinta.
4. Los botones llaman a los comandos del proveedor (`Request Purchase`...) y `Set Loading(true)`. La respuesta llega a `BP_OnCommandResult`.
5. Registra la pantalla en `DA_UIScreenRegistry` y ábrela con `Push Screen` desde cualquier widget o Blueprint.

Para probar el reloj y la huida: en `WBP_ExpeditionHUD`, al activarse, haz cast del proveedor a `NDUIMockDataProvider` y llama a `Start Simulated Night` con 60 s.

## Pendiente para la etapa 2

Crear todos los WBP del flujo (`Content/_NDE_/UI/Screens/...`, GDD 32.4) con datos simulados y navegación con mando y teclado. Si el MCP de Unreal funciona en el PC, parte de estos pasos se pueden automatizar.
