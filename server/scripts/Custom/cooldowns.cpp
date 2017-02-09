class player_cooldowns : public PlayerScript
{
public:
	player_cooldowns() : PlayerScript("player_cooldowns") { }

	void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/)
	{
		player->GetSpellHistory()->ModifyCooldowns(80);
	}
};

void AddSC_player_cooldowns()
{
	new player_cooldowns();
}