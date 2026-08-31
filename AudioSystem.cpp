#include "AudioSystem.h"

#include <xaudio2.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "xaudio2.lib")

namespace
{
	struct WavData
	{
		std::vector<BYTE> format;
		std::vector<BYTE> samples;
		const WAVEFORMATEX* get_format() const { return reinterpret_cast<const WAVEFORMATEX*>(format.data()); }
	};

	bool read_exact(std::ifstream& file, void* destination, std::streamsize bytes)
	{
		file.read(static_cast<char*>(destination), bytes);
		return file.good();
	}

	// RIFF/WAVEを直接読み込む。XAudio2へ渡すPCMデータは再生中もWavDataが所有する。
	bool load_wav_file(const wchar_t* filename, WavData& wav)
	{
		std::ifstream file(filename, std::ios::binary);
		if (!file) return false;
		char riff[4]{}, wave[4]{};
		uint32_t riff_size = 0;
		if (!read_exact(file, riff, 4) || !read_exact(file, &riff_size, sizeof(riff_size)) || !read_exact(file, wave, 4)) return false;
		if (std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(wave, "WAVE", 4) != 0) return false;

		bool has_format = false;
		while (file && !file.eof())
		{
			char chunk_id[4]{};
			uint32_t chunk_size = 0;
			if (!read_exact(file, chunk_id, 4) || !read_exact(file, &chunk_size, sizeof(chunk_size))) break;
			if (std::memcmp(chunk_id, "fmt ", 4) == 0)
			{
				if (chunk_size < 16) return false;
				wav.format.assign(std::max<size_t>(chunk_size, sizeof(WAVEFORMATEX)), 0);
				if (!read_exact(file, wav.format.data(), chunk_size)) return false;
				has_format = true;
			}
			else if (std::memcmp(chunk_id, "data", 4) == 0)
			{
				wav.samples.resize(chunk_size);
				if (chunk_size > 0 && !read_exact(file, wav.samples.data(), chunk_size)) return false;
			}
			else
			{
				file.seekg(chunk_size, std::ios::cur);
				if (!file) return false;
			}
			if (chunk_size & 1) file.seekg(1, std::ios::cur);
		}
		return has_format && !wav.samples.empty();
	}
}

class AudioSystem::Impl
{
public:
	Microsoft::WRL::ComPtr<IXAudio2> engine;
	IXAudio2MasteringVoice* mastering_voice = nullptr;
	IXAudio2SourceVoice* bgm_voice = nullptr;
	std::unordered_map<std::wstring, WavData> loaded_wavs;
	std::wstring playing_bgm_filename;
	std::vector<IXAudio2SourceVoice*> effect_voices;
	float bgm_volume = 0.65f;
};

AudioSystem::AudioSystem() = default;
AudioSystem::~AudioSystem() { shutdown(); }

bool AudioSystem::initialize()
{
	if (impl) return true;
	try
	{
		impl = std::make_unique<Impl>();
		if (FAILED(XAudio2Create(impl->engine.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR))) { impl.reset(); return false; }
		if (FAILED(impl->engine->CreateMasteringVoice(&impl->mastering_voice))) { impl.reset(); return false; }
		return true;
	}
	catch (...) { impl.reset(); return false; }
}

void AudioSystem::update()
{
	if (!impl) return;
	for (auto it = impl->effect_voices.begin(); it != impl->effect_voices.end();)
	{
		XAUDIO2_VOICE_STATE state{};
		(*it)->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
		if (state.BuffersQueued == 0) { (*it)->DestroyVoice(); it = impl->effect_voices.erase(it); }
		else ++it;
	}
}

void AudioSystem::shutdown()
{
	if (!impl) return;
	stop_bgm();
	for (IXAudio2SourceVoice* voice : impl->effect_voices) voice->DestroyVoice();
	impl->effect_voices.clear();
	if (impl->mastering_voice) impl->mastering_voice->DestroyVoice();
	impl->mastering_voice = nullptr;
	impl->engine.Reset();
	impl.reset();
}

bool AudioSystem::play_bgm(const wchar_t* filename, float volume)
{
	if (!filename || !initialize()) return false;
	volume = std::clamp(volume, 0.0f, 1.0f);
	if (impl->playing_bgm_filename == filename) { set_bgm_volume(volume); return true; }
	WavData& wav = impl->loaded_wavs[filename];
	if (wav.samples.empty() && !load_wav_file(filename, wav)) { impl->loaded_wavs.erase(filename); return false; }

	stop_bgm();
	if (FAILED(impl->engine->CreateSourceVoice(&impl->bgm_voice, wav.get_format()))) return false;
	XAUDIO2_BUFFER buffer{};
	buffer.AudioBytes = static_cast<UINT32>(wav.samples.size());
	buffer.pAudioData = wav.samples.data();
	buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	if (FAILED(impl->bgm_voice->SubmitSourceBuffer(&buffer)) || FAILED(impl->bgm_voice->SetVolume(volume)) || FAILED(impl->bgm_voice->Start()))
	{
		impl->bgm_voice->DestroyVoice(); impl->bgm_voice = nullptr; return false;
	}
	impl->playing_bgm_filename = filename;
	impl->bgm_volume = volume;
	return true;
}

void AudioSystem::stop_bgm()
{
	if (!impl) return;
	if (impl->bgm_voice)
	{
		impl->bgm_voice->Stop();
		impl->bgm_voice->FlushSourceBuffers();
		impl->bgm_voice->DestroyVoice();
		impl->bgm_voice = nullptr;
	}
	impl->playing_bgm_filename.clear();
}

void AudioSystem::set_bgm_volume(float volume)
{
	if (!impl) return;
	impl->bgm_volume = std::clamp(volume, 0.0f, 1.0f);
	if (impl->bgm_voice) impl->bgm_voice->SetVolume(impl->bgm_volume);
}

bool AudioSystem::play_sound_effect(const wchar_t* filename, float volume)
{
	if (!filename || !initialize()) return false;
	WavData& wav = impl->loaded_wavs[filename];
	if (wav.samples.empty() && !load_wav_file(filename, wav)) { impl->loaded_wavs.erase(filename); return false; }
	IXAudio2SourceVoice* voice = nullptr;
	if (FAILED(impl->engine->CreateSourceVoice(&voice, wav.get_format()))) return false;
	XAUDIO2_BUFFER buffer{};
	buffer.AudioBytes = static_cast<UINT32>(wav.samples.size());
	buffer.pAudioData = wav.samples.data();
	if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->SetVolume(std::clamp(volume, 0.0f, 1.0f))) || FAILED(voice->Start()))
	{
		voice->DestroyVoice(); return false;
	}
	impl->effect_voices.push_back(voice);
	return true;
}
