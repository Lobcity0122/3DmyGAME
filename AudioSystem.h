#pragma once

#include <memory>

// XAudio2（DirectX Tool Kit経由）を利用するゲーム用の音声窓口。
// BGMと効果音の再生をシーン側から分離し、音声実装の詳細を隠す。
class AudioSystem
{
public:
	AudioSystem();
	~AudioSystem();
	AudioSystem(const AudioSystem&) = delete;
	AudioSystem& operator=(const AudioSystem&) = delete;

	bool initialize();
	void update();
	void shutdown();

	// 同じファイルが既に再生中なら再スタートしない。BGMは1曲だけループ再生する。
	bool play_bgm(const wchar_t* filename, float volume = 0.65f);
	void stop_bgm();
	void set_bgm_volume(float volume);

	// 短い効果音は重ねて再生できる。ファイルは初回だけ読み込み、以後はキャッシュを使う。
	bool play_sound_effect(const wchar_t* filename, float volume = 1.0f);

private:
	class Impl;
	std::unique_ptr<Impl> impl;
};
