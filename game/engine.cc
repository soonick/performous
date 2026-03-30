#include "engine.hh"

#include "analyzer.hh"
#include "audio.hh"
#include "configuration.hh"
#include "database.hh"
#include "song.hh"
#include <iostream>
#include <list>

const double Engine::TIMESTEP = 1.00;

Engine::Engine(Audio &audio, VocalTrackPtrs vocals, Database &database)
    : m_audio(audio), m_time(), m_quit(), m_database(database) {
  auto &analyzers = m_audio.analyzers();
  if (analyzers.size() != vocals.size())
    throw std::logic_error("Engine requires the same number of vocal tracks as "
                           "there are analyzers.");
  // Clear old player information
  m_database.cur.clear();
  m_database.scores.clear();
  unsigned i = 0;
  for (Analyzer &a : analyzers) {
    // Calculate the space required for pitch frames
    size_t frames = static_cast<size_t>(vocals[i]->endTime / Engine::TIMESTEP);
    m_database.cur.push_back(Player(*vocals[i], a, frames));
    ++i;
  }
  m_thread.reset(new std::thread(std::ref(*this)));
}

void Engine::operator()() {
  // while (!m_quit) {
  //   for (Player &player : m_database.cur) {
  //     player.prepare();
  //   }
  //   double t = m_audio.getPosition() - config["audio/round-trip"].f();
  //   if (t < 0.0) {
  //     std::chrono::duration duration = std::min(TIMESTEP, t * -1) * 1s;
  //     SpdLogger::error(LogSystem::WEBCAM, "t = {}. Going to sleep for {}s", t, duration.count());
  //     std::this_thread::sleep_for(duration);
  //     continue;
  //   }

  //   double timeLeft = m_time - t;
  //   if (timeLeft != timeLeft || timeLeft > 1.0) {
  //     // FIXME: Workaround for NaN values and other weirdness
  //     // (should fix the weirdness instead)
  //     SpdLogger::error(LogSystem::WEBCAM, "--------Time left weirdness: {}, m_time: {}, t: {}", timeLeft, m_time, t);
  //     timeLeft = 1.0;
  //   }
  //   if (timeLeft > 0.0) {
  //     std::chrono::duration duration = std::min(TIMESTEP, timeLeft) * 1s;
  //     // SpdLogger::error(LogSystem::WEBCAM, "---------Going to sleep for {}s", duration.count());
  //     std::this_thread::sleep_for(duration);
  //     continue;
  //   }
  //   for (Player &player : m_database.cur) {
  //     player.update();
  //   }
  //   m_time += TIMESTEP;
  // }

  double lastPos = 0.0;
  while (!m_quit) {
    for (Player& player: m_database.cur) {
      player.prepare();
    }
    double t = m_audio.getPosition() - config["audio/round-trip"].f();

    // This happens when the game just started, before the song starts playing
    if (t < 0.0) {
      std::chrono::duration duration = std::min(TIMESTEP, t * -1) * 1s;
      SpdLogger::error(LogSystem::WEBCAM, "t = {}. Going to sleep for {}s", t, duration.count());
      std::this_thread::sleep_for(duration);
      continue;
    }

    int calculated = (int)(t / TIMESTEP);
    if (t < lastPos) {
      SpdLogger::error(LogSystem::WEBCAM, "---------Back seek detected");
      for (Player& player: m_database.cur) {
        for (std::size_t i = calculated; i < player.m_pitch.size() - 1; ++i) {
          player.m_pitch[i].first = 0.0;
          player.m_pitch[i].second = 0.0;
        }
      }
    }
    lastPos = t;

    // m_time = calculated * TIMESTEP;
    double timeLeft = m_time - t;
    // SpdLogger::error(LogSystem::WEBCAM, "---------Rel m_time: {}, calculated: {}, timeLeft: {}", m_time, calculated, timeLeft);
    if (timeLeft != timeLeft || timeLeft > 1.0) {
      // FIXME: Workaround for NaN values and other weirdness
      // (should fix the weirdness instead)
      timeLeft = 1.0;
    }
    if (timeLeft > 0.0) {
      // SpdLogger::error(LogSystem::WEBCAM, "---------Sleeping for: {}", std::min(TIMESTEP, timeLeft));
      std::this_thread::sleep_for(std::min(TIMESTEP, timeLeft) * 1s);
      continue;
    }

    for (Player &player : m_database.cur) {
      // player.m_pos = calculated;
      player.update();
    }
    m_time += TIMESTEP;
  }
}
