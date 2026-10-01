# DLC en el port (Survival Arenas, máscaras, etc.)

Splatterhouse (2010) tuvo contenido descargable (DLC). El port lo soporta: si el
usuario tiene los paquetes, se pueden jugar. Esta guía explica cómo funciona y
cómo colocarlos.

## Qué DLC hay

- **Survival Arena Pack**: 4 arenas (`island_arena`, `heartboss_arena`,
  `mirror_rick_arena`, `white_tile_arena`) — el `info.xml` las declara con
  `<Flag name="Is DLC" val="1"/>`.
- **Máscaras / armas / trajes** extra (contenido cosmético y de gameplay).
- Nota: el **disco base ya incluye** las 6 Survival Arenas originales y las
  emulaciones *Classic Splatterhouse 1/2/3*; el DLC añade arenas y objetos
  adicionales.

## Cómo funciona (cadena completa)

1. En Xbox 360 el DLC era un **paquete STFS** (firma `CON `/`LIVE`/`PIRS`) que la
   consola montaba como `XContent`. Su `info.xml` declara niveles
   (`DownloadPackage` → `<Level ...><Path val="levels/...">`).
2. El engine (`DownloadManager` / `DownloadManagerXenon` →
   `LevelDatabase::AddDLCLevel`) enumeraba el contenido (`XEnumerate`), abría el
   paquete, leía `info.xml` y **añadía los niveles DLC al `leveldb`**.
3. El runtime de ReXGlue **ya implementa** `XamContent*` (`ContentManager`:
   enumerar, crear, abrir/montar, licencias) y monta el paquete como device VFS
   con su symlink (`XContent:…`). Al abrirlo, el engine lee el `info.xml` del
   paquete montado y añade los niveles.

Lo único que faltaba: **instalar** los paquetes STFS en la carpeta de contenido
del usuario. El port lo hace automáticamente al arrancar.

## Instalación automática (implementado)

En `SplatterhouseApp::InstallDlcPackages()` (llamado desde
`OnPostLoadXexImage`, cuando ya se conoce el title ID) el port:

1. Busca paquetes STFS en:
   - `%USERPROFILE%\Documents\splatterhouse\<title>\00000002\`
   - `%USERPROFILE%\Documents\splatterhouse\0000000000000000\<title>\00000002\`
   - **cualquier subcarpeta de primer nivel** (p. ej. el xuid del perfil
     `B13EBABEBABEBABE`) `\<title>\00000002\`
   
   donde `<title>` es `4E4D07F0`.
2. Para cada paquete, `ContentManager::InstallContentFromDirectory` lo detecta
   por su magic (`CON `/`LIVE`/`PIRS`) y lo instala con `InstallContent`:
   - lo monta con `StfsContainerDevice` y **extrae** su contenido a
     `root_path_/0000000000000000/<title>/00000002/<filename>/`
   - escribe la **cabecera** `.header` para la enumeración XAM
     (`…/<title>/Headers/00000002/<filename>.header`)
   - usa la **license mask** de las licencias del paquete STFS
3. Es idempotente: si el paquete ya está extraído, se salta.

Tras esto, el engine hace `XamContentCreateEnumerator(type=0x00000002)` → ve los
paquetes, los abre y añade sus niveles/máscaras al menú.

## Cómo colocarlo (usuario)

1. Consigue los paquetes DLC (formato STFS: ficheros que empiezan por
   `CON `/`LIVE`/`PIRS`, normalmente sin extensión o `.xcp`).
2. Cópialos a, por ejemplo:
   ```
   %USERPROFILE%\Documents\splatterhouse\4E4D07F0\00000002\
   ```
   (también vale bajo cualquier `<xuid>\4E4D07F0\00000002\`).
3. Arranca el port. En el log verás:
   ```
   Installed DLC package <hash>
   [sh-dlc] Instalados N paquete(s) DLC desde la carpeta de usuario
   ```
4. En el menú aparecerán las arenas/objetos DLC.

## Diagnóstico

- **`REX_SH_DLC=1`** (variable de entorno): loguea en detalle qué contenido
  enumera/abre el juego (`[sh-dlc] XamContentCreateEnumerator type=…`,
  `XamContentCreate type=… file=…`) y el `ListContent`. Útil si un DLC no
  aparece.
- El log de instalación (`Installed DLC package …`) va a nivel `info`; con
  `log_level = "warn"` no se muestra, pero la instalación ocurre igual.

## Limitaciones / notas

- El DLC cosmético (máscaras/trajes) usa `DLCBodyPart` y el sistema de
  condiciones del save; en el port se desbloquea con el sistema de condiciones
  (las arenas DLC se ven en el selector; las máscaras, al elegir capítulo).
- `XamContentOpenFile` sigue siendo un stub en el SDK; no ha hecho falta porque
  el engine lee los ficheros del paquete por el VFS montado, no por esa API.
- Licencias: el port copia la license mask del propio paquete STFS, así que el
  contenido descargado legítimamente se reconoce sin parchear nada.
