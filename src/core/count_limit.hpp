#pragma once

namespace uagfx::countlimit {

// 3.0.24-beta.22 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, «Скільки штук»: «предел по весу
// и месту: «у склад влізе 87»»): how many of a stack of `stack` fit where it goes - one's weight `unitWeight` in
// tenths (-1 not known: no weight's limit), the weight free there `weightFree` in tenths (-1: none kept there - the
// storage), the places free `placesFree` (-1 not counted) and whether its own kind lies there to stack on
int most(int stack, int unitWeight, int weightFree, int placesFree, bool stacksThere);

}  // namespace uagfx::countlimit
