#pragma once

#include <algorithm>
#include <fstream>

// Small, deliberately transparent save format for data that must survive a
// program restart. The current game has only one persistent value, so one
// text file is easier to inspect and reset than introducing a larger system.
namespace GameSave
{
	inline constexpr const char* high_score_file_name = "circuit_trax_high_score.txt";

	inline int load_high_score()
	{
		int score = 0;
		std::ifstream input(high_score_file_name);
		if (input) input >> score;
		return (std::max)(score, 0);
	}

	inline void save_high_score(int score)
	{
		std::ofstream output(high_score_file_name, std::ios::trunc);
		if (output) output << (std::max)(score, 0) << '\n';
	}
}
