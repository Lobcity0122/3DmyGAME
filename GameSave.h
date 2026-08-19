#pragma once

#include <algorithm>
#include <fstream>

// 実行をまたいで残す小さなゲームデータを扱う。
// 現在はハイスコアだけなので、人間が確認・リセットしやすいテキストファイルを使う。
namespace GameSave
{
	inline constexpr const char* high_score_file_name = "circuit_trax_high_score.txt";

	// ファイルが存在しない、または不正な値だった場合は0点として開始する。
	inline int load_high_score()
	{
		int score = 0;
		std::ifstream input(high_score_file_name);
		if (input) input >> score;
		return (std::max)(score, 0);
	}

	// 常に最新のハイスコア1件だけを書き込む。
	inline void save_high_score(int score)
	{
		std::ofstream output(high_score_file_name, std::ios::trunc);
		if (output) output << (std::max)(score, 0) << '\n';
	}
}
