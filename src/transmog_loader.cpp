/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 * Copyright (C) 2021+ WarheadCore <https://github.com/WarheadCore>
 */

// From SC
void AddSC_Transmog();
//void AddSC_transmog_commandscript();
void AddSC_Transmog_Item();
void AddSC_Transmog_Player();

// Add all
void Addmod_transmogScripts()
{
    AddSC_Transmog();
    //AddSC_transmog_commandscript();
    //AddSC_Transmog_Item();
    AddSC_Transmog_Player();
}
