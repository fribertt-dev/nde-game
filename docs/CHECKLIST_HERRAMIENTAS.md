# Lista de comprobación de herramientas (PC de Felipe)

Ejecuta cada comando en una terminal (PowerShell o similar) y anota el resultado.

| Herramienta | Cómo comprobarla | Esperado |
|---|---|---|
| Git | `git --version` | 2.40 o superior |
| Git LFS | `git lfs version` y luego `git lfs install` | git-lfs 3.x, "Git LFS initialized" |
| Cuenta GitHub en Git | `git config --global user.name` y `user.email` | Tu nombre y correo de GitHub |
| Autenticación GitHub | `gh auth status` (si usas GitHub CLI) o un `git clone` de prueba | Sesión iniciada |
| Unreal Engine 5.7 | Epic Games Launcher > Biblioteca | 5.7.x instalado |
| Visual Studio 2022 (si habrá C++) | Visual Studio Installer | "Desarrollo de juegos con C++" + SDK de Windows |
| Blender | Ayuda > Acerca de Blender | 4.x estable |
| Espacio en disco | Explorador | 150 GB libres o más |

## Notas
- Activa en UE 5.7 el plugin de control de versiones Git (Editor > Source Control > Git LFS 2) para usar bloqueo de archivos binarios.
- Los `.uasset`, `.umap` y `.blend` van marcados como `lockable` en `.gitattributes` para evitar conflictos entre varias personas.
- La cuota gratuita de GitHub LFS es de 10 GB de almacenamiento y ancho de banda al mes; un juego UE la supera pronto, así que conviene prever un paquete de datos.
