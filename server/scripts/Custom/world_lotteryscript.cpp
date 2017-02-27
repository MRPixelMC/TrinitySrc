#define	ITEM_TICKETS	777313

void handleLottery()
{
	// Init variables
	uint32 rewardID;
	uint16 rewardCount;
	uint32 totalTickets = 0;
	uint32 chosenNumber;
	PreparedQueryResult res;
	char msg[255];
	std::random_device rd;
	std::default_random_engine generator(rd());

	// Send first message, telling players a winner is being chosen
	sprintf(msg, "|cffff4e50[Lottery] |cff0088b2Choosing a winner...");
	sWorld->SendGlobalText(msg, NULL);
	
	// Run query to select the item entry + quantity this lottery is using as reward
	PreparedStatement* selectReward = WorldDatabase.GetPreparedStatement(WORLD_SEL_LOTTERY_REWARD);
	res = WorldDatabase.Query(selectReward);
	if (res)
	{
		Field* fields = res->Fetch();
		rewardID = fields[0].GetUInt32();
		rewardCount = fields[1].GetUInt16();
	}
	else
		return;
	
	/* 
	   #############################################################################################################
	   # How it works:																							   #
	   # A query is ran, result of which contains 'owner_guid' & 'count', the struct 'PlayerTickets' contains	   #
	   # the following data: playerGuid, itemCount, numberInterval. 'playerGuid' holds player's GUID; itemCount	   #
	   # holds the quantity of tickets that player has; numberInterval holds an interval between two numbers	   #
	   # that will serve to check if that player is the winner. The number interval length is determined by the	   #
	   # itemCount of the player, so a player with more tickets has a higher chance to win.						   #
	   # The system then chooses a number from 0 to totalTickets (sum of all player's itemCount) and checks who's  #
	   # interval has this number, then makes him the winner of the lottery.									   #
	   #############################################################################################################
	*/
	struct PlayerTickets {
		uint32 playerGuid;
		uint32 itemCount;
		std::pair<uint32, uint32> numberInterval;
	} pTickets[1024]; // 1024 players max (low or ok?)
	
	/* fields[0] = owner_guid; fields[1] = count */
	PreparedStatement* selectGUIDs = CharacterDatabase.GetPreparedStatement(CHAR_SEL_LOTTERY_ITEMGUIDS);
	res = CharacterDatabase.Query(selectGUIDs);
	if (res)
	{
		uint32 currentInterval = 0;
		pTickets[0].playerGuid = 0; // Set this to 0, if it doesn't change then number of participants = 0
		for (int i = 0; i < res->GetRowCount(); ++i, res->NextRow())
		{
			Field* fields = res->Fetch();
			pTickets[i].playerGuid = fields[0].GetUInt32();
			pTickets[i].itemCount = fields[1].GetUInt32();
			pTickets[i].numberInterval.first = currentInterval;
			pTickets[i].numberInterval.second = currentInterval + (pTickets[i].itemCount - 1);
			currentInterval += pTickets[i].itemCount;
			totalTickets += pTickets[i].itemCount;
		}
	}
	else
		return;

	if (pTickets[0].playerGuid == 0)
	{
		sprintf(msg, "|cffff4e50[Lottery] |cff0088b2There is no participants, lottery is being delayed one day.");
		sWorld->SendGlobalText(msg, NULL);
		time_t nextLottery = sWorld->getWorldState(WS_LOTTERY_TIME) + 86400;
		sWorld->setWorldState(WS_LOTTERY_TIME, nextLottery);
		return;
	}

	PlayerTickets winner;

	std::uniform_int_distribution<int> chooseNumber(0, totalTickets);
	chosenNumber = chooseNumber(generator);
	for (int i = 0; i < res->GetRowCount(); ++i)
	{
		if (chosenNumber >= pTickets[i].numberInterval.first && chosenNumber <= pTickets[i].numberInterval.second)
		{
			winner = pTickets[i];
			break;
		}
	}

	// Send second message, telling players which player won what item and quantity
	const CharacterInfo* cInfo = sWorld->GetCharacterInfo(ObjectGuid(HighGuid::Player, winner.playerGuid));
	sprintf(msg, "|cffff4e50[Lottery] |cff0088b2The winner of the lottery is... |cffff4e50%s|cff0088b2! Congratulations on winning |cffff4e50%dx %s|cff0088b2!", cInfo->Name.c_str(), rewardCount, sObjectMgr->GetItemTemplate(rewardID)->Name1.c_str());
	sWorld->SendGlobalText(msg, NULL);

	// Delete the tickets from each player
	SQLTransaction ticketDeletion = CharacterDatabase.BeginTransaction();
	for (int i = 0; i < res->GetRowCount(); ++i)
	{
		Player* player = ObjectAccessor::FindConnectedPlayer(ObjectGuid(HighGuid::Player, pTickets[i].playerGuid));
		if (player) // Player online
			player->DestroyItemCount(ITEM_TICKETS, pTickets[i].itemCount, true);
		else // Player offline
		{
			PreparedStatement* deleteTickets = CharacterDatabase.GetPreparedStatement(CHAR_DEL_LOTTERY_TICKETS);
			deleteTickets->setUInt32(0, pTickets[i].playerGuid);
			ticketDeletion->Append(deleteTickets);
		}
	}
	CharacterDatabase.CommitTransaction(ticketDeletion);

	// Send winner the reward by mail
	MailSender sender(MAIL_CREATURE, 333668);
	MailDraft draft("You've won the lottery!", "Congratulations on winning today's lottery!$B$B Here's your reward.");
	SQLTransaction sendMail = CharacterDatabase.BeginTransaction();

	Item* item = Item::CreateItem(rewardID, rewardCount, nullptr);
	if (item)
	{
		item->SaveToDB(sendMail);
		draft.AddItem(item);
	}

	draft.SendMailTo(sendMail, MailReceiver(winner.playerGuid), sender);
	CharacterDatabase.CommitTransaction(sendMail);
	
	// Select a reward from the table of lottery rewards that is NOT the same as before, goes from lower chance to higher
	// Make a struct that will hold item IDs that have 100% chance (otherwise it will always choose the same 100% chance one)
	struct SureChanceItems {
		uint32 entry;
		uint16 minCount;
		uint16 maxCount;
	} items[255];
	int sureChanceItems = 0;

	std::uniform_real_distribution<float> distribution(0, 100);

	/* fields[0] = entry; fields[1] = chance; fields[2] = mincount; fields[3] = maxcount */
	PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_LOTTERY_REWARDLIST);
	res = WorldDatabase.Query(stmt);
	if (res)
	{
		do {
			Field* fields = res->Fetch();
			if (fields[1].GetFloat() >= 100)
			{
				items[sureChanceItems].entry = fields[0].GetUInt32();
				items[sureChanceItems].minCount = fields[2].GetUInt16();
				items[sureChanceItems].maxCount = fields[3].GetUInt16();
				sureChanceItems++;
				continue;
			}

			float randNum = roundf(distribution(generator) * 100) / 100; // Rounds float to 2 decimal places
			if (randNum <= fields[1].GetFloat())
			{
				rewardID = fields[0].GetUInt32();
				if (fields[2].GetUInt16() != fields[3].GetUInt16())
				{
					std::uniform_int_distribution<int> selectCount(fields[2].GetUInt16(), fields[3].GetUInt16());
					rewardCount = selectCount(generator);
				}
				else
					rewardCount = fields[2].GetUInt16();

				break;
			}
		} while (res->NextRow());
	}

	if (sureChanceItems != 0)
	{
		std::uniform_int_distribution<int> selectItem(0, sureChanceItems - 1);
		int number = selectItem(generator);
		rewardID = items[number].entry;
		if (items[number].minCount != items[number].maxCount)
		{
			std::uniform_int_distribution<int> selectCount(items[number].minCount, items[number].maxCount);
			rewardCount = selectCount(generator);
		}
		else
			rewardCount = items[number].minCount;
	}
	
	SQLTransaction updateRewards = WorldDatabase.BeginTransaction();
	PreparedStatement* stmt1 = WorldDatabase.GetPreparedStatement(WORLD_UPD_LOTTERY_REMOVEREWARD);
	PreparedStatement* stmt2 = WorldDatabase.GetPreparedStatement(WORLD_UPD_LOTTERY_ADDREWARD);
	stmt2->setUInt16(0, rewardCount);
	stmt2->setUInt32(1, rewardID);
	updateRewards->Append(stmt1);
	updateRewards->Append(stmt2);
	WorldDatabase.CommitTransaction(updateRewards);

	// Send third message, telling players a new lottery is up and the reward
	sprintf(msg, "|cffff4e50[Lottery] |cff0088b2The new lottery is up! The reward is: |cffff4e50%dx %s|cff0088b2!", rewardCount, sObjectMgr->GetItemTemplate(rewardID)->Name1.c_str());
	sWorld->SendGlobalText(msg, NULL);
};

class world_lotteryscript : public WorldScript
{
public:
	world_lotteryscript() : WorldScript("world_lotteryscript") { }

	void OnStartup()
	{
		m_lotteryTime = sWorld->getWorldState(WS_LOTTERY_TIME);
		m_lotteryTimeChecker = 600000; // 10 minutes
		if (m_lotteryTime == 0)
		{
			m_lotteryTime = time(0) + 86400;
			sWorld->setWorldState(WS_LOTTERY_TIME, m_lotteryTime);
		}
	}

	void OnUpdate(uint32 diff)
	{
		if (m_lotteryTimeChecker < diff)
		{
			if (time(0) >= m_lotteryTime)
			{
				handleLottery();
				m_lotteryTime = sWorld->getWorldState(WS_LOTTERY_TIME) + 86400;
				sWorld->setWorldState(WS_LOTTERY_TIME, m_lotteryTime);
			}
			m_lotteryTimeChecker = 600000;
		}
		else
			m_lotteryTimeChecker -= diff;
	}

private:
	uint32 m_lotteryTime;
	uint32 m_lotteryTimeChecker;
};

class lottery_commandscript : public CommandScript
{
public:
	lottery_commandscript() : CommandScript("lottery_commandscript") { }

	std::vector<ChatCommand> GetCommands() const override
	{
		static std::vector<ChatCommand> commandTable =
		{
			{ "lottery",          rbac::RBAC_PERM_COMMAND_HELP,          false, &HandleLotteryCommand,          "" },
		};
		return commandTable;
	}

	static bool HandleLotteryCommand(ChatHandler* handler, char const* args)
	{
		// Get the time when next lottery will be drawn
		int32 timeInSeconds = difftime(sWorld->getWorldState(WS_LOTTERY_TIME), time(NULL));

		if (timeInSeconds <= 0)
		{
			handler->PSendSysMessage("Lottery will be drawn shortly (max. 10 minutes).");
			return true;
		}

		int seconds = timeInSeconds % 60;
		int minutes = (timeInSeconds % 3600) / 60;
		int hours = timeInSeconds / 3600;

		// Get player tickets + calculate his chance of winning
		Player* player = handler->GetSession()->GetPlayer();
		int ticketCount = player->GetItemCount(ITEM_TICKETS, true);
		PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_LOTTERY_TOTALTICKETS);
		PreparedQueryResult res = CharacterDatabase.Query(stmt);
		if (res)
		{
			Field* fields = res->Fetch();
			float chanceFloat = ((float)ticketCount / (float)fields[0].GetUInt32()) * 100;
			int chance = std::ceil(chanceFloat);
			handler->PSendSysMessage("Lottery will be drawn in %d hours, %d minutes, %d seconds.", hours, minutes, seconds);
			handler->PSendSysMessage("You have %d tickets (%d%% chance of winning).", ticketCount, chance);
		}
		return true;
	}
};

void AddSC_world_lotteryscript()
{
	new world_lotteryscript();
	new lottery_commandscript();
}