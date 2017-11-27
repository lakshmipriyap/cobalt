// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/media/base/starboard_player.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/logging.h"
#include "cobalt/media/base/shell_media_platform.h"
#include "cobalt/media/base/starboard_utils.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {

StarboardPlayer::CallbackHelper::CallbackHelper(StarboardPlayer* player)
    : player_(player) {}

void StarboardPlayer::CallbackHelper::ClearDecoderBufferCache() {
  base::AutoLock auto_lock(lock_);
  if (player_) {
    player_->ClearDecoderBufferCache();
  }
}

void StarboardPlayer::CallbackHelper::OnDecoderStatus(
    SbPlayer player, SbMediaType type, SbPlayerDecoderState state, int ticket) {
  base::AutoLock auto_lock(lock_);
  if (player_) {
    player_->OnDecoderStatus(player, type, state, ticket);
  }
}

void StarboardPlayer::CallbackHelper::OnPlayerStatus(SbPlayer player,
                                                     SbPlayerState state,
                                                     int ticket) {
  base::AutoLock auto_lock(lock_);
  if (player_) {
    player_->OnPlayerStatus(player, state, ticket);
  }
}

void StarboardPlayer::CallbackHelper::OnDeallocateSample(
    const void* sample_buffer) {
  base::AutoLock auto_lock(lock_);
  if (player_) {
    player_->OnDeallocateSample(sample_buffer);
  }
}

void StarboardPlayer::CallbackHelper::ResetPlayer() {
  base::AutoLock auto_lock(lock_);
  player_ = NULL;
}

StarboardPlayer::StarboardPlayer(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const AudioDecoderConfig& audio_config,
    const VideoDecoderConfig& video_config, SbWindow window,
    SbDrmSystem drm_system, Host* host,
    SbPlayerSetBoundsHelper* set_bounds_helper, bool prefer_decode_to_texture)
    : message_loop_(message_loop),
      callback_helper_(
          new CallbackHelper(ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      audio_config_(audio_config),
      video_config_(video_config),
      window_(window),
      drm_system_(drm_system),
      host_(host),
      set_bounds_helper_(set_bounds_helper),
      frame_width_(1),
      frame_height_(1),
      ticket_(SB_PLAYER_INITIAL_TICKET),
      volume_(1.0),
      playback_rate_(0.0),
      seek_pending_(false),
      state_(kPlaying) {
  DCHECK(audio_config.IsValidConfig());
  DCHECK(video_config.IsValidConfig());
  DCHECK(host_);
  DCHECK(set_bounds_helper_);

  output_mode_ = ComputeSbPlayerOutputMode(
      MediaVideoCodecToSbMediaVideoCodec(video_config.codec()), drm_system,
      prefer_decode_to_texture);

  CreatePlayer();

  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::CallbackHelper::ClearDecoderBufferCache,
                 callback_helper_));
}

StarboardPlayer::~StarboardPlayer() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  callback_helper_->ResetPlayer();
  set_bounds_helper_->SetPlayer(NULL);

  ShellMediaPlatform::Instance()->GetVideoFrameProvider()->SetOutputMode(
      ShellVideoFrameProvider::kOutputModeInvalid);
  ShellMediaPlatform::Instance()
      ->GetVideoFrameProvider()
      ->ResetGetCurrentSbDecodeTargetFunction();

  if (SbPlayerIsValid(player_)) {
    SbPlayerDestroy(player_);
  }
}

void StarboardPlayer::UpdateVideoResolution(int frame_width, int frame_height) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  frame_width_ = frame_width;
  frame_height_ = frame_height;
}

void StarboardPlayer::WriteBuffer(DemuxerStream::Type type,
                                  const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // When |state_| is kPlaying, cache all buffer appended.  When |state_| is
  // kSuspended, there may still be left-over buffers appended from the pipeline
  // so they also should be cached.
  // When |state_| is resuming, all buffers come from the cache and shouldn't be
  // cached.
  if (state_ != kResuming) {
    decoder_buffer_cache_.AddBuffer(type, buffer);
  }

  if (state_ == kSuspended) {
    DCHECK(!SbPlayerIsValid(player_));
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  if (buffer->end_of_stream()) {
    SbPlayerWriteEndOfStream(player_, DemuxerStreamTypeToSbMediaType(type));
    return;
  }

  const auto& allocations = buffer->allocations();
  DCHECK_GT(allocations.number_of_buffers(), 0);

  DecodingBuffers::iterator iter =
      decoding_buffers_.find(allocations.buffers()[0]);
  if (iter == decoding_buffers_.end()) {
    decoding_buffers_[allocations.buffers()[0]] = std::make_pair(buffer, 1);
  } else {
    ++iter->second.second;
  }

  SbDrmSampleInfo drm_info;
  SbDrmSubSampleMapping subsample_mapping;
  bool is_encrypted = buffer->decrypt_config();
  SbMediaVideoSampleInfo video_info;

  drm_info.subsample_count = 0;
  video_info.is_key_frame = buffer->is_key_frame();
  video_info.frame_width = frame_width_;
  video_info.frame_height = frame_height_;

  SbMediaColorMetadata sb_media_color_metadata =
      MediaToSbMediaColorMetadata(video_config_.webm_color_metadata());
  video_info.color_metadata = &sb_media_color_metadata;

  if (is_encrypted) {
    FillDrmSampleInfo(buffer, &drm_info, &subsample_mapping);
  }

  SbPlayerWriteSample(player_, DemuxerStreamTypeToSbMediaType(type),
#if SB_API_VERSION >= SB_PLAYER_WRITE_SAMPLE_EXTRA_CONST_API_VERSION
                      allocations.buffers(), allocations.buffer_sizes(),
#else   // SB_API_VERSION >= SB_PLAYER_WRITE_SAMPLE_EXTRA_CONST_API_VERSION
                      const_cast<const void**>(allocations.buffers()),
                      const_cast<int*>(allocations.buffer_sizes()),
#endif  // SB_API_VERSION >= SB_PLAYER_WRITE_SAMPLE_EXTRA_CONST_API_VERSION
                      allocations.number_of_buffers(),
                      TimeDeltaToSbMediaTime(buffer->timestamp()),
                      type == DemuxerStream::VIDEO ? &video_info : NULL,
                      drm_info.subsample_count > 0 ? &drm_info : NULL);
}

void StarboardPlayer::SetBounds(int z_index, const gfx::Rect& rect) {
  if (state_ == kSuspended) {
    DCHECK(!SbPlayerIsValid(player_));
    pending_set_bounds_z_index_ = z_index;
    pending_set_bounds_rect_ = rect;
    return;
  }

  DCHECK(SbPlayerIsValid(player_));
  SbPlayerSetBounds(player_, z_index, rect.x(), rect.y(), rect.width(),
                    rect.height());
}

void StarboardPlayer::PrepareForSeek() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  seek_pending_ = true;

  if (state_ == kSuspended) {
    DCHECK(!SbPlayerIsValid(player_));
    return;
  }

  ++ticket_;
  SbPlayerSetPlaybackRate(player_, 0.f);
}

void StarboardPlayer::Seek(base::TimeDelta time) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  decoder_buffer_cache_.ClearAll();

  if (state_ == kSuspended) {
    DCHECK(!SbPlayerIsValid(player_));
    preroll_timestamp_ = time;
    return;
  }

  // If a seek happens during resuming, the pipeline will write samples from the
  // seek target time again so resuming can be aborted.
  if (state_ == kResuming) {
    state_ = kPlaying;
  }

  DCHECK(SbPlayerIsValid(player_));

  ++ticket_;
  SbPlayerSeek(player_, TimeDeltaToSbMediaTime(time), ticket_);
  seek_pending_ = false;
  SbPlayerSetPlaybackRate(player_, playback_rate_);
}

void StarboardPlayer::SetVolume(float volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  volume_ = volume;

  if (state_ == kSuspended) {
    DCHECK(!SbPlayerIsValid(player_));
    return;
  }

  DCHECK(SbPlayerIsValid(player_));
  SbPlayerSetVolume(player_, volume);
}

void StarboardPlayer::SetPlaybackRate(double playback_rate) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  playback_rate_ = playback_rate;

  if (state_ == kSuspended) {
    DCHECK(!SbPlayerIsValid(player_));
    return;
  }

  if (seek_pending_) {
    return;
  }

  SbPlayerSetPlaybackRate(player_, playback_rate);
}

void StarboardPlayer::GetInfo(uint32* video_frames_decoded,
                              uint32* video_frames_dropped,
                              base::TimeDelta* media_time) {
  DCHECK(video_frames_decoded || video_frames_dropped || media_time);

  base::AutoLock auto_lock(lock_);
  if (state_ == kSuspended) {
    DCHECK(!SbPlayerIsValid(player_));

    if (video_frames_decoded) {
      *video_frames_decoded = cached_video_frames_decoded_;
    }
    if (video_frames_dropped) {
      *video_frames_dropped = cached_video_frames_dropped_;
    }
    if (media_time) {
      *media_time = preroll_timestamp_;
    }
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerInfo info;
  SbPlayerGetInfo(player_, &info);
  if (video_frames_decoded) {
    *video_frames_decoded = info.total_video_frames;
  }
  if (video_frames_dropped) {
    *video_frames_dropped = info.dropped_video_frames;
  }
  if (media_time) {
    *media_time = SbMediaTimeToTimeDelta(info.current_media_pts);
  }
}

void StarboardPlayer::Suspend() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Check if the player is already suspended.
  if (state_ == kSuspended) {
    DCHECK(!SbPlayerIsValid(player_));
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerSetPlaybackRate(player_, 0.0);

  base::AutoLock auto_lock(lock_);

  state_ = kSuspended;

  SbPlayerInfo info;
  SbPlayerGetInfo(player_, &info);
  cached_video_frames_decoded_ = info.total_video_frames;
  cached_video_frames_dropped_ = info.dropped_video_frames;
  preroll_timestamp_ = SbMediaTimeToTimeDelta(info.current_media_pts);

  set_bounds_helper_->SetPlayer(NULL);
  SbPlayerDestroy(player_);

  player_ = kSbPlayerInvalid;
}

void StarboardPlayer::Resume() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Check if the player is already resumed.
  if (state_ != kSuspended) {
    DCHECK(SbPlayerIsValid(player_));
    return;
  }

  DCHECK(!SbPlayerIsValid(player_));

  decoder_buffer_cache_.StartResuming();

  CreatePlayer();

  base::AutoLock auto_lock(lock_);
  state_ = kResuming;
}

namespace {
ShellVideoFrameProvider::OutputMode ToVideoFrameProviderOutputMode(
    SbPlayerOutputMode output_mode) {
  switch (output_mode) {
    case kSbPlayerOutputModeDecodeToTexture:
      return ShellVideoFrameProvider::kOutputModeDecodeToTexture;
    case kSbPlayerOutputModePunchOut:
      return ShellVideoFrameProvider::kOutputModePunchOut;
    case kSbPlayerOutputModeInvalid:
      return ShellVideoFrameProvider::kOutputModeInvalid;
  }

  NOTREACHED();
  return ShellVideoFrameProvider::kOutputModeInvalid;
}
}  // namespace

void StarboardPlayer::CreatePlayer() {
  TRACE_EVENT0("cobalt::media", "StarboardPlayer::CreatePlayer");
  DCHECK(message_loop_->BelongsToCurrentThread());

  SbMediaAudioHeader audio_header =
      MediaAudioConfigToSbMediaAudioHeader(audio_config_);

  SbMediaAudioCodec audio_codec =
      MediaAudioCodecToSbMediaAudioCodec(audio_config_.codec());
  SbMediaVideoCodec video_codec =
      MediaVideoCodecToSbMediaVideoCodec(video_config_.codec());

  DCHECK(SbPlayerOutputModeSupported(output_mode_, video_codec, drm_system_));

  player_ = SbPlayerCreate(window_, video_codec, audio_codec,
                           SB_PLAYER_NO_DURATION, drm_system_, &audio_header,
                           &StarboardPlayer::DeallocateSampleCB,
                           &StarboardPlayer::DecoderStatusCB,
                           &StarboardPlayer::PlayerStatusCB, this, output_mode_,
                           ShellMediaPlatform::Instance()
                               ->GetSbDecodeTargetGraphicsContextProvider());
  DCHECK(SbPlayerIsValid(player_));

  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    // If the player is setup to decode to texture, then provide Cobalt with
    // a method of querying that texture.
    ShellMediaPlatform::Instance()
        ->GetVideoFrameProvider()
        ->SetGetCurrentSbDecodeTargetFunction(
            base::Bind(&StarboardPlayer::GetCurrentSbDecodeTarget,
                       base::Unretained(this)));
  }
  ShellMediaPlatform::Instance()->GetVideoFrameProvider()->SetOutputMode(
      ToVideoFrameProviderOutputMode(output_mode_));

  set_bounds_helper_->SetPlayer(this);

  if (pending_set_bounds_z_index_ && pending_set_bounds_rect_) {
    SetBounds(*pending_set_bounds_z_index_, *pending_set_bounds_rect_);
    pending_set_bounds_z_index_ = base::nullopt_t();
    pending_set_bounds_rect_ = base::nullopt_t();
  }
}

SbDecodeTarget StarboardPlayer::GetCurrentSbDecodeTarget() {
  return SbPlayerGetCurrentFrame(player_);
}

SbPlayerOutputMode StarboardPlayer::GetSbPlayerOutputMode() {
  return output_mode_;
}

void StarboardPlayer::ClearDecoderBufferCache() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ != kResuming) {
    base::TimeDelta media_time;
    GetInfo(NULL, NULL, &media_time);
    decoder_buffer_cache_.ClearSegmentsBeforeMediaTime(media_time);
  }

  message_loop_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::CallbackHelper::ClearDecoderBufferCache,
                 callback_helper_),
      base::TimeDelta::FromMilliseconds(
          kClearDecoderCacheIntervalInMilliseconds));
}

void StarboardPlayer::OnDecoderStatus(SbPlayer player, SbMediaType type,
                                      SbPlayerDecoderState state, int ticket) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (player_ != player || ticket != ticket_) {
    return;
  }

  DCHECK_NE(state_, kSuspended);

  switch (state) {
    case kSbPlayerDecoderStateNeedsData:
      break;
    case kSbPlayerDecoderStateBufferFull:
      DLOG(WARNING) << "kSbPlayerDecoderStateBufferFull has been deprecated.";
      return;
    case kSbPlayerDecoderStateDestroyed:
      return;
  }

  if (state_ == kResuming) {
    DemuxerStream::Type stream_type = SbMediaTypeToDemuxerStreamType(type);
    if (decoder_buffer_cache_.GetBuffer(stream_type)) {
      WriteBuffer(stream_type, decoder_buffer_cache_.GetBuffer(stream_type));
      decoder_buffer_cache_.AdvanceToNextBuffer(stream_type);
      return;
    }
    if (!decoder_buffer_cache_.GetBuffer(DemuxerStream::AUDIO) &&
        !decoder_buffer_cache_.GetBuffer(DemuxerStream::VIDEO)) {
      state_ = kPlaying;
    }
  }

  host_->OnNeedData(SbMediaTypeToDemuxerStreamType(type));
}

void StarboardPlayer::OnPlayerStatus(SbPlayer player, SbPlayerState state,
                                     int ticket) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (player_ != player) {
    return;
  }

  DCHECK_NE(state_, kSuspended);

  if (ticket != SB_PLAYER_INITIAL_TICKET && ticket != ticket_) {
    return;
  }

  if (state == kSbPlayerStateInitialized) {
    if (ticket_ == SB_PLAYER_INITIAL_TICKET) {
      ++ticket_;
    }
    SbPlayerSeek(player_, TimeDeltaToSbMediaTime(preroll_timestamp_), ticket_);
    SetVolume(volume_);
    SbPlayerSetPlaybackRate(player_, playback_rate_);
    return;
  }
  host_->OnPlayerStatus(state);
}

void StarboardPlayer::OnDeallocateSample(const void* sample_buffer) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  DecodingBuffers::iterator iter = decoding_buffers_.find(sample_buffer);
  DCHECK(iter != decoding_buffers_.end());
  if (iter == decoding_buffers_.end()) {
    LOG(ERROR) << "StarboardPlayer::OnDeallocateSample encounters unknown "
               << "sample_buffer " << sample_buffer;
    return;
  }
  --iter->second.second;
  if (iter->second.second == 0) {
    decoding_buffers_.erase(iter);
  }
}

// static
void StarboardPlayer::DecoderStatusCB(SbPlayer player, void* context,
                                      SbMediaType type,
                                      SbPlayerDecoderState state, int ticket) {
  StarboardPlayer* helper = reinterpret_cast<StarboardPlayer*>(context);
  helper->message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::CallbackHelper::OnDecoderStatus,
                 helper->callback_helper_, player, type, state, ticket));
}

// static
void StarboardPlayer::PlayerStatusCB(SbPlayer player, void* context,
                                     SbPlayerState state, int ticket) {
  StarboardPlayer* helper = reinterpret_cast<StarboardPlayer*>(context);
  helper->message_loop_->PostTask(
      FROM_HERE, base::Bind(&StarboardPlayer::CallbackHelper::OnPlayerStatus,
                            helper->callback_helper_, player, state, ticket));
}

// static
void StarboardPlayer::DeallocateSampleCB(SbPlayer player, void* context,
                                         const void* sample_buffer) {
  StarboardPlayer* helper = reinterpret_cast<StarboardPlayer*>(context);
  helper->message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::CallbackHelper::OnDeallocateSample,
                 helper->callback_helper_, sample_buffer));
}

// static
SbPlayerOutputMode StarboardPlayer::ComputeSbPlayerOutputMode(
    SbMediaVideoCodec codec, SbDrmSystem drm_system,
    bool prefer_decode_to_texture) {
  // Try to choose the output mode according to the passed in value of
  // |prefer_decode_to_texture|.  If the preferred output mode is unavailable
  // though, fallback to an output mode that is available.
  SbPlayerOutputMode output_mode = kSbPlayerOutputModeInvalid;
  if (SbPlayerOutputModeSupported(kSbPlayerOutputModePunchOut, codec,
                                  drm_system)) {
    output_mode = kSbPlayerOutputModePunchOut;
  }
  if ((prefer_decode_to_texture || output_mode == kSbPlayerOutputModeInvalid) &&
      SbPlayerOutputModeSupported(kSbPlayerOutputModeDecodeToTexture, codec,
                                  drm_system)) {
    output_mode = kSbPlayerOutputModeDecodeToTexture;
  }
  CHECK_NE(kSbPlayerOutputModeInvalid, output_mode);

#if defined(COBALT_ENABLE_LIB)
  // When Cobalt is used as a library, it doesn't control the
  // screen and so punch-out cannot be assumed.  You will need
  // to implement decode-to-texture support if you wish to use
  // Cobalt as a library and play videos with it.
  CHECK_EQ(kSbPlayerOutputModeDecodeToTexture, output_mode)
#endif  // defined(COBALT_ENABLE_LIB)

  return output_mode;
}

}  // namespace media
}  // namespace cobalt