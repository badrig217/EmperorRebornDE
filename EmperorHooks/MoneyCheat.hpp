#pragma once
// Optional single-player money cheat: Ctrl+Shift+G adds 25,000 credits to the human player.
// Never active in multiplayer. Enabled from the launcher ("Extras").
void initMoneyCheat(bool enabled);
void moneyCheatUpdateFrame(); // call once per rendered frame (from EndScene)
