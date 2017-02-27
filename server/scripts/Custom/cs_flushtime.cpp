class flushtime_commandscript : public CommandScript
{
public:
	flushtime_commandscript() : CommandScript("flushtime_commandscript") { }

	std::vector<ChatCommand> GetCommands() const override
	{
		static std::vector<ChatCommand> commandTable =
		{
			{ "flushtime",          rbac::RBAC_PERM_COMMAND_HELP,          false, &HandleFlushtimeCommand,          "" },
		};
		return commandTable;
	}

	static bool HandleFlushtimeCommand(ChatHandler* handler, char const* args)
	{
		int32 timeInSeconds = difftime(sWorld->getWorldState(WS_ARENA_DISTRIBUTION_TIME), time(NULL));
		
		if (timeInSeconds <= 0)
		{
			handler->PSendSysMessage("The arena flush has happened recently.");
			return true;
		}

		int seconds = timeInSeconds % 60;
		int minutes = (timeInSeconds % 3600) / 60;
		int hours = timeInSeconds / 3600;
		int days = 0;
		while (hours >= 24)
		{
			hours -= 24;
			days++;
		}
		handler->PSendSysMessage("Time left for next arena flush: %d days, %d hours, %d minutes, %d seconds.", days, hours, minutes, seconds);
		return true;
	}
};

void AddSC_flushtime_commandscript()
{
	new flushtime_commandscript();
}