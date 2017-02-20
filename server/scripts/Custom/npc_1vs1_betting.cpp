#include "npc_1vs1_betting.h"

class npc_1vs1_betting : public CreatureScript
{
public:
	npc_1vs1_betting() : CreatureScript("npc_1vs1_betting") { }

	bool OnGossipHello(Player* player, Creature* creature)
	{
		player->PlayerTalkClass->GetGossipMenu().AddMenuItem(-1, NULL, "I want to challenge someone.", GOSSIP_SENDER_MAIN, 1, "Insert player name.", 0, true);
		player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
		return true;
	}

	bool OnGossipSelectCode(Player* player, Creature* creature, uint32 /* sender */, uint32 action, const char* code)
	{
		player->PlayerTalkClass->ClearMenus();

		if (action > 10)
		{
			count = atoi(code);
			if (count < 1 || count > 5000)
			{
				ChatHandler(player->GetSession()).PSendSysMessage("|cff6E9ECF%d is not a valid number.", count);
				player->PlayerTalkClass->SendCloseGossip();
				return false;
			}

			if (!player->HasItemCount(action, count))
			{
				ChatHandler(player->GetSession()).PSendSysMessage("|cff6E9ECFYou don't have |cffE9514C%dx %s|cff6E9ECF.", count, sObjectMgr->GetItemTemplate(action)->Name1);
				player->PlayerTalkClass->SendCloseGossip();
				return false;
			}

			// Send info to DB table here
			PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_INS_1VS1);
			stmt->setUInt32(0, player->GetGUID());
			stmt->setUInt32(1, pTarget->GetGUID());
			stmt->setUInt32(2, action);
			stmt->setUInt32(3, count);
			stmt->setUInt32(4, 0); // Arena set to 0, will be updated if duel is accepted
			stmt->setUInt32(5, 0); // Phase set to 0, will be updated if duel is accepted
			WorldDatabase.Execute(stmt);

			// Send player messages and open gossip for pTarget
			ChatHandler(player->GetSession()).PSendSysMessage("|cff6E9ECFYou have challenged |cffE9514C%s |cff6E9ECFto a duel for |cffE9514C%dx %s|cff6E9ECF!", pTarget->GetName(), count, sObjectMgr->GetItemTemplate(action)->Name1);
			ChatHandler(pTarget->GetSession()).PSendSysMessage("|cffE9514C%s |cff6E9ECFhas challenged you to a duel for |cffE9514C%dx %s|cff6E9ECF!", player->GetName(), count, sObjectMgr->GetItemTemplate(action)->Name1);
			pTarget->PlayerTalkClass->SendCloseGossip();
			pTarget->PlayerTalkClass->ClearMenus();
			pTarget->PlayerTalkClass->GetGossipMenu().AddMenuItem(-1, NULL, "Accept", GOSSIP_SENDER_MAIN, OPTION_ACCEPT_DUEL, "", 0);
			pTarget->PlayerTalkClass->GetGossipMenu().AddMenuItem(-1, NULL, "Decline", GOSSIP_SENDER_MAIN, OPTION_DECLINE_DUEL, "", 0);
			pTarget->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, pTarget->GetGUID());

			// If pTarget selects a gossip option the DuelExpire events will be removed from both players
			// preventing said events from removing the row from the DB Table
			new DuelExpireSender(player);
			new DuelExpireReceiver(pTarget);
			player->PlayerTalkClass->SendCloseGossip();
			return true;
		}

		switch (action)
		{
			case 1:
				std::string name(code);
				pTarget = ObjectAccessor::FindConnectedPlayerByName(name);

				// If target not found
				if (!pTarget)
				{
					ChatHandler(player->GetSession()).PSendSysMessage("|cff6E9ECFPlayer |cffE9514C%s |cff6E9ECFdoesn't exist or isn't online.", name);
					player->PlayerTalkClass->SendCloseGossip();
					return false;
				}
				
				// If target is player
				if (pTarget == player)
				{
					ChatHandler(player->GetSession()).PSendSysMessage("|cff6E9ECFYou can't challenge yourself.");
					player->PlayerTalkClass->SendCloseGossip();
					return false;
				}

				// If target is a GM
				if (pTarget->IsGameMaster())
					return false;

				// If target is either AFK or DND
				if (pTarget->isAFK() || pTarget->isDND())
				{
					ChatHandler(player->GetSession()).PSendSysMessage("|cff6E9ECFPlayer |cffE9514C%s |cff6E9ECFis AFK or DND.", name);
					player->PlayerTalkClass->SendCloseGossip();
					return false;
				}

				// If target is in a battleground or arena
				if (pTarget->InBattleground() || pTarget->InArena())
				{
					ChatHandler(player->GetSession()).PSendSysMessage("|cff6E9ECFPlayer |cffE9514C%s |cff6E9ECFis in a Battleground.", name);
					player->PlayerTalkClass->SendCloseGossip();
					return false;
				}

				// If target is in combat
				if (pTarget->IsInCombat())
				{
					ChatHandler(player->GetSession()).PSendSysMessage("|cff6E9ECFPlayer |cffE9514C%s |cff6E9ECFis in combat.", name);
					player->PlayerTalkClass->SendCloseGossip();
					return false;
				}

				PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_1VS1_BYGUID);
				stmt->setUInt32(0, player->GetGUID());
				stmt->setUInt32(1, player->GetGUID());
				PreparedQueryResult res = WorldDatabase.Query(stmt);

				if (res)
				{
					Field* fields = res->Fetch();
					Player* pl1 = ObjectAccessor::FindConnectedPlayer(ObjectGuid(HighGuid::Player, fields[0].GetUInt32()));
					Player* pl2 = ObjectAccessor::FindConnectedPlayer(ObjectGuid(HighGuid::Player, fields[1].GetUInt32()));

					if (pl1)
					{
						ChatHandler(pl1->GetSession()).PSendSysMessage("|cff6E9ECFYour current duel request has been cancelled.");
						removeEvents(pl1);
						if (pl1 == player)
						{
							clearDuel(player, true);
							if (pl2)
								pl2->PlayerTalkClass->SendCloseGossip();
						}
					}
					if (pl2)
					{
						ChatHandler(pl2->GetSession()).PSendSysMessage("|cff6E9ECFYour current duel request has been cancelled.");
						removeEvents(pl2);
						if (pl2 == player)
						{
							clearDuel(player, false);
							if (pl1)
								pl1->PlayerTalkClass->SendCloseGossip();
						}
					}
				}

				for (int i = 0; i < MAX_ITEMS; ++i)
					player->PlayerTalkClass->GetGossipMenu().AddMenuItem(-1, NULL, sObjectMgr->GetItemTemplate(available_items[i])->Name1, GOSSIP_SENDER_MAIN, available_items[i], "Insert quantity.", 0, true);

				player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
				break;
		}
		return true;
	}

private:
	Player* pTarget;
	uint32 count;
};

class player_1vs1_betting : public PlayerScript
{
	public:
		player_1vs1_betting() : PlayerScript("player_1vs1_betting") { }

		void OnGossipSelect(Player* player, uint32 menuID, uint32 /*sender*/, uint32 action)
		{
			player->PlayerTalkClass->ClearMenus();
			player->PlayerTalkClass->SendCloseGossip();

			// Current player is being challenged so we search for 'player2' column
			PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_1VS1_BYGUID);
			stmt->setUInt32(0, 0);
			stmt->setUInt32(1, player->GetGUID());
			PreparedQueryResult res = WorldDatabase.Query(stmt);

			if (res)
			{
				Field* fields = res->Fetch();
				pDueler = ObjectAccessor::FindConnectedPlayer(ObjectGuid(HighGuid::Player, fields[0].GetUInt32()));
				pTarget = ObjectAccessor::FindConnectedPlayer(ObjectGuid(HighGuid::Player, fields[1].GetUInt32()));
				itemID = fields[2].GetUInt32();
				count = fields[3].GetUInt32();
			}

			if (!pDueler || !pTarget)
			{
				ChatHandler(player->GetSession()).PSendSysMessage("|cff6E9ECFA player has disconnected, duel cancelled.");
				clearDuel(player, false);
				removeEvents(player);
				return;
			}

			// Delete the events so the DB Table row doesn't get deleted automatically
			removeEvents(pDueler);
			removeEvents(pTarget);

			switch (action)
			{
				// Player accepts
				case OPTION_ACCEPT_DUEL:
					if (!player->HasItemCount(itemID, count))
					{
						ChatHandler(player->GetSession()).PSendSysMessage("|cff6E9ECFYou don't have |cffE9514C%dx %s|cff6E9ECF.", count, sObjectMgr->GetItemTemplate(itemID)->Name1);
						ChatHandler(pDueler->GetSession()).PSendSysMessage("|cff6E9ECFPlayer |cffE9514C%s |cff6E9ECF doesn't have the required items.", player->GetName());
						clearDuel(player, false);
						return;
					}

					for (int j = 0; j < MAX_PHASES; ++j)
					{
						for (int i = 0; i < MAX_MAPS; ++i)
						{
							Player* temp = sWorld->FindPlayerInZone(maps[i].zoneID);
							if (!temp)
							{
								// This means there's 0 players in this zone so we choose it
								arena = i;
								break;
							}

							int playercount = 0;
							Map* map = temp->GetMap();
							Map::PlayerList const& players = map->GetPlayers();
							for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
							{
								if (itr->GetSource()->IsGameMaster())
									continue;

								if (itr->GetSource()->GetAreaId() != maps[i].areaID)
									continue;

								if (itr->GetSource()->GetPhaseMask() != phases[j])
									continue;

								playercount++;
							}

							if (playercount > 0)
								continue;

							// 0 Players so we choose this
							arena = i;
							break;
						}

						if (arena != 10000)
						{
							phase = phases[j];
							break;
						}
					}

					if (arena == 10000)
					{
						ChatHandler(pDueler->GetSession()).PSendSysMessage("|cff6E9ECFThe duel has been cancelled because all arenas are currently busy.");
						ChatHandler(pTarget->GetSession()).PSendSysMessage("|cff6E9ECFThe duel has been cancelled because all arenas are currently busy.");
						clearDuel(player, false);
						return;
					}

					// Update arena & phase in the DB
					stmt = WorldDatabase.GetPreparedStatement(WORLD_UPD_1VS1_BYGUID);
					stmt->setUInt32(0, arena);
					stmt->setUInt32(1, phase);
					stmt->setUInt32(2, pDueler->GetGUID());
					stmt->setUInt32(3, player->GetGUID());
					WorldDatabase.Execute(stmt);

					// Do all the stuff, set phase and send players to wherever it is
					pDueler->ResetAllPowers();
					pTarget->ResetAllPowers();
					pDueler->RemoveArenaAuras();
					pTarget->RemoveArenaAuras();
					pDueler->RemoveArenaSpellCooldowns();
					pTarget->RemoveArenaSpellCooldowns();
					pDueler->SetPhaseMask(phase, true);
					pTarget->SetPhaseMask(phase, true);

					// Freeze buff BEFORE teleporting!
					pDueler->AddAura(9454, pDueler);
					pTarget->AddAura(9454, pTarget);
					new DuelFreezeBuffRemoval(pDueler);
					new DuelFreezeBuffRemoval(pTarget);
					ChatHandler(pDueler->GetSession()).PSendSysMessage("|cff6E9ECFDuel will start in 5 seconds!");
					ChatHandler(pTarget->GetSession()).PSendSysMessage("|cff6E9ECFDuel will start in 5 seconds!");
					pDueler->TeleportTo(WorldLocation(maps[arena].mapID, maps[arena].player1Spawn));
					pTarget->TeleportTo(WorldLocation(maps[arena].mapID, maps[arena].player2Spawn));
					return;
					break;

				// Player declines
				case OPTION_DECLINE_DUEL:
					ChatHandler(pDueler->GetSession()).PSendSysMessage("|cff6E9ECFPlayer |cffE9514C%s |cff6E9ECFhas declined your duel.", player->GetName());
					ChatHandler(player->GetSession()).PSendSysMessage("|cff6E9ECFYou have declined the duel.");
					clearDuel(player, false);
					return;
					break;
			}
		}

		void OnPVPKill(Player* player, Player* target)
		{
			PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_1VS1_BYGUID);
			stmt->setUInt32(0, player->GetGUID());
			stmt->setUInt32(1, player->GetGUID());
			PreparedQueryResult res = WorldDatabase.Query(stmt);
			
			if (!res)
				return;
			
			Field* fields = res->Fetch();
			itemID = fields[2].GetUInt32();
			count = fields[3].GetUInt32();
			arena = fields[4].GetUInt32();
			phase = fields[5].GetUInt32();

			if (player->GetZoneId() == maps[arena].zoneID && player->GetAreaId() == maps[arena].areaID && player->GetPhaseMask() == phase)
			{
				player->AddItem(itemID, count);
				target->DestroyItemCount(itemID, count, true);

				char msg[255];
				sprintf(msg, "|cffE9514C%s |cff6E9ECFwon a duel against |cffE9514C%s |cff6E9ECFfor |cffE9514C%dx %s|cff6E9ECF!", player->GetName().c_str(), target->GetName().c_str(), count, sObjectMgr->GetItemTemplate(itemID)->Name1.c_str());
				sWorld->SendGlobalText(msg, NULL);

				player->SetPhaseMask(1, true);
				player->RemoveArenaSpellCooldowns();
				player->ResetAllPowers();
				player->Recall();

				target->ResurrectPlayer(100);
				target->SetPhaseMask(1, true);
				target->RemoveArenaSpellCooldowns();
				target->ResetAllPowers();
				target->Recall();

				if (player->GetGUID() == ObjectGuid(HighGuid::Player, fields[0].GetUInt32()))
					clearDuel(player, true);
				else
					clearDuel(player, false);
			}
		}

		void OnLogin(Player* player, bool firstLogin)
		{
			if (firstLogin)
				return;

			PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_1VS1_BYGUID);
			stmt->setUInt32(0, player->GetGUID());
			stmt->setUInt32(1, player->GetGUID());
			PreparedQueryResult res = WorldDatabase.Query(stmt);

			if (res)
			{
				do {
					Field* fields = res->Fetch();
					Player* pl1 = ObjectAccessor::FindConnectedPlayer(ObjectGuid(HighGuid::Player, fields[0].GetUInt32()));
					// Dont need pl2 because the SELECT query returned so either player is player1 or player2 no choice

					if (player == pl1)
						clearDuel(player, true);
					else
						clearDuel(player, false);
				} while (res->NextRow());
			}

			return;
		}

	private:
		Player* pDueler;
		Player* pTarget;
		uint32 itemID;
		uint32 count;
		uint32 arena = 10000; // Using 10000 as a token to know if value has changed or not
		uint32 phase;
};

void AddSC_npc_1vs1_betting()
{
	new npc_1vs1_betting();
	new player_1vs1_betting();
}