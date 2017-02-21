#define OPTION_SHOWLIST	1
#define OPTION_DOUBLE	2
#define OPTION_TRIPLE	3

class npc_gamble : public CreatureScript
{
	public:
		npc_gamble() : CreatureScript("npc_gamble") { }

		bool OnGossipHello(Player* player, Creature* creature)
		{
			player->PlayerTalkClass->GetGossipMenu().AddMenuItem(-1, NULL, "Sure, show me the list!", GOSSIP_SENDER_MAIN, OPTION_SHOWLIST, "", 0);

			// "Aye! Up for some gambling? Bet me anything from this list!"
			player->PlayerTalkClass->SendGossipMenu(333666, creature->GetGUID());
			return true;
		}

		bool OnGossipSelect(Player* player, Creature* creature, uint32 /* sender */, uint32 option)
		{
			player->PlayerTalkClass->ClearMenus();
			PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_GAMBLE_ITEMS);
			PreparedQueryResult res = WorldDatabase.Query(stmt);
			
			switch (option)
			{
				case OPTION_SHOWLIST:
					// Show list of items (grab from DB table)
					if (res)
					{
						for (int i = 0; i < res->GetRowCount(); i++)
						{
							Field* fields = res->Fetch();
							PreparedStatement* stmt2 = WorldDatabase.GetPreparedStatement(WORLD_SEL_ITEMNAME_BYENTRY);
							stmt2->setUInt32(0, fields[0].GetUInt32());
							PreparedQueryResult res2 = WorldDatabase.Query(stmt2);

							if (res2)
							{
								Field* fields2 = res2->Fetch();
								player->PlayerTalkClass->GetGossipMenu().AddMenuItem(-1, NULL, fields2[0].GetString(), GOSSIP_SENDER_MAIN, fields[0].GetUInt32(), "", 0);
							}
						}
					}

					// "Choose any of these items, I've got lots of 'em! What 'bout ya?"
					player->PlayerTalkClass->SendGossipMenu(333667, creature->GetGUID());
					return true;
					break;
			}

			if (option >= 10)
			{
				itemID = option;
				player->PlayerTalkClass->GetGossipMenu().AddMenuItem(-1, NULL, "Double!", GOSSIP_SENDER_MAIN, OPTION_DOUBLE, "Insert item quantity", 0, true);
				player->PlayerTalkClass->GetGossipMenu().AddMenuItem(-1, NULL, "Triple!", GOSSIP_SENDER_MAIN, OPTION_TRIPLE, "Insert item quantity", 0, true);
				
				// "Double or triple? Double's 50% chance for you to win the item twice! Triple's 30% chance for you but ya'll get the item thrice!"
				player->PlayerTalkClass->SendGossipMenu(333668, creature->GetGUID());
				return true;
			}

			return true;
		}

		bool OnGossipSelectCode(Player* player, Creature* creature, uint32 /* sender */, uint32 action, const char* code)
		{
			int number = atoi(code);
			if (number < 1)
			{
				creature->Whisper("I don't do deals with less than 1 item!", LANG_UNIVERSAL, player);
				player->PlayerTalkClass->SendCloseGossip();
				return false;
			}
			else if (number > 5000)
			{
				creature->Whisper("I'm not willing to risk that much, I have a max of 5000!", LANG_UNIVERSAL, player);
				player->PlayerTalkClass->SendCloseGossip();
				return false;
			}

			if (!player->HasItemCount(itemID, number))
			{
				creature->Whisper("You don't have that! Don't go around wasting my time!", LANG_UNIVERSAL, player);
				player->PlayerTalkClass->SendCloseGossip();
				return false;
			}


			player->DestroyItemCount(itemID, number, true);

			srand(time(0));
			int n = rand() % 100;
			int c;

			switch (action)
			{
			case OPTION_DOUBLE:
				c = 50;
				if (n > c)
				{
					ChatHandler(player->GetSession()).PSendSysMessage("The number is: %d, you win!", c);
					creature->Whisper("Ah, you're a lucky one ain't ya?", LANG_UNIVERSAL, player);
					player->AddItem(itemID, number * 2);
				}
				else
				{	
					ChatHandler(player->GetSession()).PSendSysMessage("The number is: %d, you lose!", c);
					creature->Whisper("Heh, better luck next time.", LANG_UNIVERSAL, player);
				}
				break;

			case OPTION_TRIPLE:
				c = 70;
				if (n > c)					{
					ChatHandler(player->GetSession()).PSendSysMessage("The number is: %d, you win!", c);
					creature->Whisper("Really?! No way, you must've cheated!", LANG_UNIVERSAL, player);
					player->AddItem(itemID, number * 3);
				}
				else
				{
					ChatHandler(player->GetSession()).PSendSysMessage("The number is: %d, you lose!", c);
					creature->Whisper("Gambling, the sure way of getting nothing for something!", LANG_UNIVERSAL, player);
				}
				break;
			}
			
			player->PlayerTalkClass->SendCloseGossip();
			return true;
		}

	private:
		uint32 itemID;
};

void AddSC_npc_gamble()
{
	new npc_gamble();
}