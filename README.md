# Progress Keeper

Progress Keeper adds a small floating save button to Geometry Dash menus. The button can be dragged anywhere on the screen and remembers its position.

Press the button to create a backup immediately or schedule recurring backups in seconds, minutes, or hours. The button is hidden while a level is running. If an interval ends during gameplay, the backup waits until the player returns to a menu.

Backups are stored locally in the mod save directory. The newest 20 backup folders are kept automatically. This mod does not upload account data to the Geometry Dash cloud.

## Build

The project targets Geometry Dash 2.2081 and Geode 5.8.2. Set `GEODE_SDK` to a Geode SDK installation, then configure and build the project with CMake. The included GitHub Actions workflow builds Win64, MacOS, iOS, Android32, and Android64, then combines them into one package artifact.

## License

Progress Keeper is available under the MIT License.

This mod has been made with AI.
