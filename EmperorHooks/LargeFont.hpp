#pragma once
// Optional large in-game text: switches the game's "Small" and "Standard" fonts to the Emperor Spice
// HD fonts (2x size, rendered from DejaVu Sans, shipped as extra UI/3DDATA archives that the launcher
// installs). Does nothing unless those archives are present in the DATA folder.
void initLargeFont(bool enabled);
