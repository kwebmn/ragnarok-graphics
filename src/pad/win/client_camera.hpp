#pragma once

// The uaRO client's camera, read and turned in its memory: the right stick turns it the way a right-button
// drag does, and the pad's directions follow the turn.
namespace uapad::clientcamera {

// True when the running exe is the uaRO build whose layout we know (checked by code signatures).
bool supported();
// The camera's turn on screen now, in the client's own degrees. False outside the game (login, loading).
bool readTurn(double& degrees);
// Adds `deltaDeg` to the turn the camera heads for (it gets there smoothly), within the limits the client
// keeps for a right-button drag: free outdoors, 20 degrees either way of the map's own turn indoors.
bool turn(float deltaDeg);

}  // namespace uapad::clientcamera
