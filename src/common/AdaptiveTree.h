/*
*      Copyright (C) 2016-2016 peak3d
*      http://www.peak3d.de
*
*  This Program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2, or (at your option)
*  any later version.
*
*  This Program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*  GNU General Public License for more details.
*
*  <http://www.gnu.org/licenses/>.
*
*/

#pragma once

#include <vector>
#include <string>
#include <map>
#include <inttypes.h>
#include "expat.h"

namespace adaptive
{
  template <typename T>
  struct SPINCACHE
  {
    SPINCACHE() :basePos(0) {};

    size_t basePos;

    const T *operator[](uint32_t pos) const
    {
      if (!~pos)
        return 0;
      size_t realPos = basePos + pos;
      if (realPos >= data.size())
      {
        realPos -= data.size();
        if (realPos == basePos)
          return 0;
      }
      return &data[realPos];
    };

    uint32_t pos(const T* elem) const
    {
      size_t realPos = elem - &data[0];
      if (realPos < basePos)
        realPos += data.size() - basePos;
      else
        realPos -= basePos;
      return static_cast<std::uint32_t>(realPos);
    };

    void insert(const T &elem)
    {
      data[basePos] = elem;
      ++basePos;
      if (basePos == data.size())
        basePos = 0;
    }

    std::vector<T> data;
  };

  class AdaptiveTree
  {
  public:
    enum StreamType
    {
      NOTYPE,
      VIDEO,
      AUDIO,
      SUBTITLE,
      STREAM_TYPE_COUNT
    };

    enum ContainerType : uint8_t
    {
      CONTAINERTYPE_NOTYPE,
      CONTAINERTYPE_MP4,
      CONTAINERTYPE_TS
    };

    // Node definition
    struct Segment
    {
      void SetRange(const char *range);
      uint64_t range_begin_; //Either byterange start or timestamp or ~0
      union
      {
        uint64_t range_end_; //Either byterange end or sequence_id or char* if range_begin is ~0
        const char *url;
      };
      uint64_t startPTS_;
    };

    struct SegmentTemplate
    {
      SegmentTemplate() :startNumber(1), timescale(0), duration(0), presentationTimeOffset(0){};
      std::string initialization;
      std::string media;
      unsigned int startNumber;
      unsigned int timescale, duration;
      uint64_t presentationTimeOffset;
    };

    struct Representation
    {
      Representation() :bandwidth_(0), samplingRate_(0),  width_(0), height_(0), fpsRate_(0), fpsScale_(1), aspect_(0.0f),
        flags_(0), hdcpVersion_(0), indexRangeMin_(0), indexRangeMax_(0), channelCount_(0), nalLengthSize_(0), pssh_set_(0),
        containerType_(AdaptiveTree::CONTAINERTYPE_MP4), duration_(0), timescale_(0), segmentBaseId_(~0ULL), nextPTS_(0){};
      std::string url_;
      std::string id;
      std::string codecs_;
      std::string codec_private_data_;
      std::string source_url_;
      uint32_t bandwidth_;
      uint32_t samplingRate_;
      uint16_t width_, height_;
      uint32_t fpsRate_, fpsScale_;
      float aspect_;
      //Flags
      static const unsigned int BYTERANGE = 0;
      static const unsigned int INDEXRANGEEXACT = 1;
      static const unsigned int TEMPLATE = 2;
      static const unsigned int TIMELINE = 4;
      static const unsigned int INITIALIZATION = 8;
      static const unsigned int SEGMENTBASE = 16;
      static const unsigned int SUBTITLESTREAM = 32;
      static const unsigned int INCLUDEDSTREAM = 64;
      static const unsigned int URLSEGMENTS = 128;

      uint16_t flags_;
      uint16_t hdcpVersion_;

      uint32_t indexRangeMin_, indexRangeMax_;
      uint8_t channelCount_, nalLengthSize_;
      uint8_t pssh_set_;
      ContainerType containerType_;
      SegmentTemplate segtpl_;
      //SegmentList
      uint32_t duration_, timescale_;
      uint64_t segmentBaseId_;
      uint64_t nextPTS_;
      Segment initialization_;
      SPINCACHE<Segment> segments_;
      const Segment *get_initialization()const { return (flags_ & INITIALIZATION) ? &initialization_ : 0; };
      const Segment *get_next_segment(const Segment *seg)const
      {
        if (!seg || seg == &initialization_)
          return segments_[0];
        else
          return segments_[segments_.pos(seg) + 1];
      };

      const Segment *get_segment(uint32_t pos)const
      {
        return ~pos ? segments_[pos] : nullptr;
      };

      const uint32_t get_segment_pos(const Segment *segment)const
      {
        return segment ? segments_.data.empty() ? 0: segments_.pos(segment) : ~0;
      }

      const uint8_t get_psshset() const
      {
        return pssh_set_;
      };

      static bool compare(const Representation* a, const Representation *b) { return a->bandwidth_ < b->bandwidth_; };

    }*current_representation_;

    struct AdaptationSet
    {
      AdaptationSet() :type_(NOTYPE), timescale_(0),  startPTS_(0) { language_ = "unk"; };
      ~AdaptationSet(){ for (std::vector<Representation* >::const_iterator b(repesentations_.begin()), e(repesentations_.end()); b != e; ++b) delete *b; };
      StreamType type_;
      uint32_t timescale_;
      uint64_t startPTS_;
      std::string language_;
      std::string mimeType_;
      std::string base_url_;
      std::string codecs_;
      std::vector<Representation*> repesentations_;
      SPINCACHE<uint32_t> segment_durations_;

      const uint32_t get_segment_duration(uint32_t pos)const
      {
        return *segment_durations_[pos];
      };

      SegmentTemplate segtpl_;
    }*current_adaptationset_;

    struct Period
    {
      Period(){};
      ~Period(){ for (std::vector<AdaptationSet* >::const_iterator b(adaptationSets_.begin()), e(adaptationSets_.end()); b != e; ++b) delete *b; };
      std::vector<AdaptationSet*> adaptationSets_;
      std::string base_url_;
    }*current_period_;

    std::vector<Period*> periods_;
    std::string base_url_, base_domain_;

    /* XML Parsing*/
    XML_Parser parser_;
    uint32_t currentNode_;
    uint32_t segcount_;
    uint64_t overallSeconds_, stream_start_, available_time_, publish_time_, base_time_;
    uint64_t minPresentationOffset;
    bool has_timeshift_buffer_;

    uint32_t bandwidth_;
    std::map<std::string, std::string> manifest_headers_;

    double download_speed_, average_download_speed_;

    std::string supportedKeySystem_;
    struct PSSH
    {
      static const uint32_t MEDIA_VIDEO = 1;
      static const uint32_t MEDIA_AUDIO = 2;

      PSSH(){};
      bool operator == (const PSSH &other) const { return pssh_ == other.pssh_ && defaultKID_ == other.defaultKID_; };
      std::string pssh_;
      std::string defaultKID_;
      std::string iv;
      uint32_t media_;
    };
    std::vector<PSSH> psshSets_;

    enum
    {
      ENCRYTIONSTATE_UNENCRYPTED = 0,
      ENCRYTIONSTATE_ENCRYPTED = 1,
      ENCRYTIONSTATE_SUPPORTED = 2
    };
    unsigned int  encryptionState_;
    uint8_t adpChannelCount_, adp_pssh_set_;
    uint16_t adpwidth_, adpheight_;
    uint32_t adpfpsRate_;
    float adpaspect_;
    bool adp_timelined_;

    bool current_hasRepURN_, current_hasAdpURN_;
    std::string current_pssh_, current_defaultKID_, current_iv_;
    std::string license_url_;

    std::string strXMLText_;

    AdaptiveTree();
    virtual ~AdaptiveTree();

    virtual bool open(const char *url) = 0;
    virtual bool prepareRepresentation(Representation *rep, uint64_t segmentId = 0) { return true; };
    virtual void OnSegmentDownloaded(Representation *rep, const Segment *seg, std::string &data) {};

    uint8_t insert_psshset(StreamType type);
    bool has_type(StreamType t);
    uint32_t estimate_segcount(uint32_t duration, uint32_t timescale);
    double get_download_speed() const { return download_speed_; };
    double get_average_download_speed() const { return average_download_speed_; };
    void set_download_speed(double speed);
    void SetFragmentDuration(const AdaptationSet* adp, const Representation* rep, size_t pos, uint64_t timestamp, uint32_t fragmentDuration, uint32_t movie_timescale);

    bool empty(){ return !current_period_ || current_period_->adaptationSets_.empty(); };
    const AdaptationSet *GetAdaptationSet(unsigned int pos) const { return current_period_ && pos < current_period_->adaptationSets_.size() ? current_period_->adaptationSets_[pos] : 0; };
protected:
  virtual bool download(const char* url, const std::map<std::string, std::string> &manifestHeaders);
  virtual bool write_data(void *buffer, size_t buffer_size) = 0;
  void SortRepresentations();
};

}
