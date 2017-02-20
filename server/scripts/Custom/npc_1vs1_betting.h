#ifndef NPC_1VS1_BETTING_H
#define NPC_1VS1_BETTING_H

#define MAX_ITEMS	9 // Increase by 1 every time an item is added in 'available_items' array
#define	MAX_MAPS	2 // Increase by 1 everytime a new map is added in 'maps' array
#define MAX_PHASES	4
#define OPTION_ACCEPT_DUEL	301
#define OPTION_DECLINE_DUEL	302

// Add item IDs here
uint32 available_items[MAX_ITEMS]
{
	14998, // Transmog Token
	37094, // Remorse Scroll Of The Berserking
	111030, // Remorse Powder
	111031, // Remorse Sphere
	111032, // Demonic Candle
	111033, // Remorse Gold
	140250, // Remorse Token of Conquest
	230024, // 10000 Honor Token
	230025 // 100000 Honor Token
};

struct ArenaInfo
{
	uint32 mapID;
	uint32 zoneID;
	uint32 areaID;
	Position player1Spawn;
	Position player2Spawn;
};

// Add places here
ArenaInfo maps[MAX_MAPS]
{
	{ 530, 3518, 3701, Position(-2070.84f, 6706.61f, 11.97f, 5.18f), Position(-2016.54f, 6602.52f, 12.28f, 2.09f) },
	{ 530, 3522, 3775, Position(2785.05f, 5902.83f, 4.97f, 0.91f), Position(2892.20f, 5954.58f, 4.98f, 4.03f) },
};

uint8 phases[MAX_PHASES]{
	16,
	32,
	64,
	128
};

// Deletes the row from the DB Table
void clearDuel(Player* player, bool player1)
{
	PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_1VS1_BYGUID);
	if (player1)
	{
		stmt->setUInt32(0, player->GetGUID());
		stmt->setUInt32(1, 0);
	}
	else
	{
		stmt->setUInt32(0, 0);
		stmt->setUInt32(1, player->GetGUID());
	}
	WorldDatabase.Execute(stmt);
	return;
};

void removeEvents(Player* player) { player->m_Events.KillAllEvents(false); };

// Event added to the duel sender when player requests duel, executes in 10 seconds
class DuelExpireSender : public BasicEvent
{
public:
	DuelExpireSender(Player* _player) : BasicEvent(), player(_player)
	{
		player->m_Events.AddEvent(this, player->m_Events.CalculateTime(10000));
	}

	bool Execute(uint64 /* e_time */, uint32 /* p_time */)
	{
		ChatHandler(player->GetSession()).PSendSysMessage("|cff6E9ECFThe duel request has expired.");
		clearDuel(player, true);
		return true;
	}

private:
	Player* player;
};

// Event added to the duel receiver when a player requests a duel with him, executes in 10 seconds
class DuelExpireReceiver : public BasicEvent
{
public:
	DuelExpireReceiver(Player* _player) : BasicEvent(), player(_player)
	{
		player->m_Events.AddEvent(this, player->m_Events.CalculateTime(10010));
	}

	bool Execute(uint64 /* e_time */, uint32 /* p_time */)
	{
		ChatHandler(player->GetSession()).PSendSysMessage("|cff6E9ECFThe duel request has expired.");
		PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_1VS1_BYGUID);
		stmt->setUInt32(0, 0);
		stmt->setUInt32(1, player->GetGUID());
		PreparedQueryResult res = WorldDatabase.Query(stmt);
		
		if (res)
			clearDuel(player, false);

		player->PlayerTalkClass->SendCloseGossip();
		return true;
	}
private:
	Player* player;
};

class DuelFreezeBuffRemoval : public BasicEvent
{
public:
	DuelFreezeBuffRemoval(Player* _player) : BasicEvent(), player(_player)
	{
		player->m_Events.AddEvent(this, player->m_Events.CalculateTime(5000));
	}

	bool Execute(uint64 /* e_time */, uint32 /* p_time */)
	{
		player->RemoveAura(9454);
		return true;
	}

private:
	Player* player;
};

#endif