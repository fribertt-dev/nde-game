# Assets 3D locales

`NDE/Content/_NDE_/Props/` (mallas, Materials y Textures importadas, unos 485 MB) **no está en el repositorio**. Vive solo en el PC de Felipe y está en `.gitignore`.

## Qué pasa en un clone limpio
Sin esa carpeta el proyecto abre y se juega igual, con el arte greybox:
- Casas procedurales (`NDHouseLot`, `NDRoomModuleActor`, muebles): cargan las mallas por ruta; si no están, quedan las cajas greybox y sale un aviso en el log.
- Furgoneta (`NDExtractionVan`) y frasco de rescate (`NDRescueContainer`): sin la malla de arte se ven sus formas greybox.
- Botín (`DA_Loot_*`): `ArtMesh` es una referencia blanda; si falta, se ve la `Mesh` básica.

## Respaldo
Git ya no guarda esta carpeta, así que el respaldo es tuyo:
1. Copia `D:\GITHUB-REPOSITORY\nde-game\NDE\Content\_NDE_\Props` a otro disco o a una nube (OneDrive, Google Drive), con el editor cerrado.
2. Repite la copia cada vez que importes o cambies mallas.
3. Los originales también están en `D:\BlenderProjects` (FBX en `NDE_FBX`); con `docs/tools/importar_meshes_ue.py` se pueden volver a importar.

## Ojo al cambiar de rama
- Al traer por primera vez el commit que saca la carpeta del repositorio (pull de `main-qa`), git **borra** `Props` del disco. Haz el respaldo antes y vuelve a copiarla después.
- Las ramas antiguas (y `main`, hasta que reciba este cambio) aún tienen la carpeta dentro. Al cambiar a ellas git puede quejarse de que "sobrescribiría archivos sin seguimiento" o volver a borrarla al salir. Ten el respaldo a mano.
